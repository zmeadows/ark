
if [ $# -eq 0 ]
  then
    perf stat -d ./simple_pos_vel
    perf stat -d ./simple_parallel_systems
    perf stat -d ./create_destroy
    exit 0
fi

perf stat -d ./$1

