// #include <consoleapi.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <handleapi.h>
#include <map>
#include <minwindef.h>
#include <processthreadsapi.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <synchapi.h>
#include <windows.h>
#include <winerror.h>
#include <winnt.h>

// -u, --udid UDID       mount specific device by UDID
int main(int argc, char *argv[]) {
  int i;
  bool udid_found = false;
  bool start;

  if (argc < 2) {
    printf("Usage: %s [start/stop] <ifuse args>\n", argv[0]);
    exit(1);
  }

  if (strncmp("start", argv[1], 5) == 0) {
    start = true;
  } else if (strncmp("stop", argv[1], 4) == 0) {
    start = false;
  } else {
    printf("Usage: %s <start/stop> <ifuse args>\n", argv[0]);
    exit(1);
  }

  for (i = 2; i < argc; i++) {
    if (strncmp("-u", argv[i], 2) == 0 || strncmp("--udid", argv[i], 6) == 0) {
      if (i != (argc - 1)) {
        udid_found = true;
        break;
      }
    }
  }

  if (!udid_found) {
    printf("Usage: expecting --udid or -u options\n");
    exit(1);
  }

  const char *new_udid = argv[i + 1];
  const char *map_file = "ifuse_map.txt";

  std::ifstream file(map_file, std::ios::in | std::ios::binary);
  if (!file) {
    std::ofstream create(map_file);
    if (!create) {
      printf("Failed to open map file\n");
      exit(1);
    }
    create.close();
    file.open(map_file, std::ios::in | std::ios::binary);
  }

  std::map<std::string, std::string> map;

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream is(line);
    std::string udid;
    std::string pid;
    is >> udid;
    is >> pid;

    if (!udid.empty() && !pid.empty()) {
      map[udid] = pid;
    }
  }

  file.close();

  if (map.count(new_udid) && start) {
    printf("Error: udid `%s` exists.\n", map[new_udid].c_str());
    exit(1);
  }

  if (start) {
    std::string cmd = ".\\ifuse.exe ";
    for (i = 2; i < argc; i++) {
      cmd.append(argv[i]);
      if (i != (argc - 1)) {
        cmd.append(" ");
      }
    }

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);

    if (CreateProcess(NULL, (char *)cmd.c_str(), NULL, NULL, FALSE, 0, NULL,
                      NULL, &si, &pi)) {
      printf("Waiting for ifuse exit (5 sec).\n");
      DWORD wait_res = WaitForSingleObject(pi.hProcess, 5000);
      if (wait_res == WAIT_TIMEOUT) {
        printf("Waiting over.\n");
        char buf[32] = {0};
        sprintf(buf, "%ld", pi.dwProcessId);
        map[new_udid] = buf;
      } else {
        printf("Ifuse exited in 5 seconds.\n");
      }
    } else {
      printf("Error: Failed to start ifuse\n");
      exit(1);
    }
  } else {
    if (map.count(new_udid) > 0) {
      DWORD pid = strtoull(map[new_udid].c_str(), NULL, 10);
      printf("Stop ifuse pid: %ld\n", pid);

      HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);

      if (proc) {
        if (TerminateProcess(proc, 1)) {
          map.erase(new_udid);
        } else {
          printf("Error failed to stop ifuse on pid %ld.\n", pid);
        }
        CloseHandle(proc);
      } else {
        printf("Warning: ifuse on pid %ld not found.\n", pid);
        map.erase(new_udid);
      }
    } else {
      printf("Warning: udid %s not found.\n", new_udid);
    }
  }

  std::ofstream ofile(map_file, std::ios::out | std::ios::trunc);
  for (const auto &[key, value] : map) {
    ofile << key + " " + value + "\n";
  }

  ofile.close();

  return 0;
}
