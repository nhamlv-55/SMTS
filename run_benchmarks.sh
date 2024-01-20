for file in $1/*.smt2; do
    abspath=$(realpath $file)
    echo $abspath
    pkill -f smts
    timeout 10s python server/smts.py -o4 -l & python server/client.py 3000 $abspath 
    ./server/client.py 3000 -t
done