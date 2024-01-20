for file in $1/*.smt2; do
    abspath=$(realpath $file)
    exp=$(basename $file)
    echo $abspath
    pkill -f smts
    python server/smts.py -o4 -l -fp $abspath > server_${exp}.log
done