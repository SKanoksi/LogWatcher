# LogWatcher
Constantly monitor a log file and watch out for specific keywords, if any appears, then execute the assigned command.

#### Limitation:
- Can only run on Linux machine (require <unistd.h> and <limits.h>)

#### Installation:
- Copy a header template, logwatcher.hpp_xxx, as logwatcher.hpp, then edit it as you see fit
- Compile using, for example, g++ -Os -o logwatcher ./logwatcher.cpp -lstdc++
- Only give x permission, i.e., chmod 711 logwatcher

#### Usage: 
(Slurm version)
```txt
Usage: logwatcher [OPTIONS]...
Constantly monitor the SLURM StdErr log file and watch out for
 1. CUDA out of memory 
 2. srun: error:       
 3. oom-kill event     
if any appears, then execute scancel to kill the job.

Note: 1) Must NOT run on frontend/login nodes
      2) Must run inside a SLURM job
      3) At most 8 logwatcher are allowed per job node

OPTIONS:
      --all                  execute when all instead of any
  -c, --catch <keyword>      keyword/text to be caught (accept several --catch)
      --check-at-start       immediately check after successfully start
      --check-once           check only once then terminate
  -e, --execute <command>    command to get executed
  -f, --logfile <filepath>   path to the log file
      --foreground           run in foreground
      --missing              execute when missing instead of found
  -n, --interval <second>    checking interval in second [>60,<3600]
  -t, --timeout <minute>     timeout duration in minute [<7200]
      --stay                 stay alive until timeout
  -v, --verbose              verbose output
  -V, --version              print version
  -h, --help                 print this usage
```
(Startup messages)
```bash
--- LOGWATCHER --- 
 PID 11111 on frontend-1
 Monitor "/home/skanoksi/test/logwatcher/software/slurm-1234.out" 
 Every 60 seconds
 It will execute "scancel 1234", 
 If any of "CUDA out of memory", "srun: error:" or "oom-kill event" is found. -- <LOGWATCHER-ignore>
 Timeout in ~60 minutes
--- LOGWATCHER --- 
```

-----

1. Constantly watch a logfile for errors
```bash
# (Explicit, no default settings)
logwatcher -e "kill -9 \$(pidof your-software-name)" -c "Out-of-Memory" -f "./software.log" -n 60 -t 60
logwatcher -e "scancel ${SLURM_JOBID}" -c "CUDA out of memory" -f "./slurm-${SLURM_JOBID}.out" -n 60 -t 60

# (use all default settings -- Slurm version)
logwatcher
logwatcher -n 60 -t 60
```

2. Check software startup process
```bash
# (Explicit, no default settings)
logwatcher -e "kill -9 \$(pidof your-software-name)" \
           -c "Waiting for connection" -c "client connected" \
           --missing -f "./software.log" \
           --check-once -n 300 --verbose

# (use all default settings -- Slurm version)
logwatcher -c "Waiting for connection" -c "client connected" \
           --missing --check-once -n 300 --verbose
```

3. Automatation 
   
3.1 To rerun software from checkpoint (e.g., CFL --> reduce dt)
```bash
logwatcher -e "./save_fix_and_rerun_xxx" \
           -c "points exceeded v_cfl" \
           -f "./software.log" \
           -n 60 -t 600
```
   
3.2 To dispatch offline coupled model (e.g., Atm -> Surface)
```bash
logwatcher -e "./run_surface_model 2024-01-01" \
           -c "Writing output_2024-01-01_00:00:00.nc" \
           -f "./software.log" \
           -n 60 -t 600
```

4. Monitor hardware utilization
```bash
logwatcher --stay -e "pdsh -w ${SLURM_NODELIST} nvidia-smi | dshbak >& jobmon-${SLURM_JOBID}.gpu" \
           --all --missing --check-at-start -n 300
logwatcher --stay -e "pdsh -w ${SLURM_NODELIST} top -b -n1 -i | dshbak >& jobmon-${SLURM_JOBID}.cpu" \
           --missing -c "PAUSE_HW_MONITORING" --check-at-start -n 300
# Note:
# 1) -n <second> should NOT be too small, so that it won't steal too much CPU time from your main workload, affecting performance.
# 2) It may be more convenient to create and use a wrapper script containing nvidia-smi, top and/or other commands.

# To see the latest snapshot of hardware utilization on those job nodes, execute
cat jobmon-xxx.gpu
cat jobmon-xxx.cpu
```

