filesize=10000
number_of_files=500
server_path=/var/tmp/omkar

N=1

while [ $N -le $number_of_files ]
do
	dd if=/dev/urandom of=$server_path/client_file_$N.txt bs=$filesize count=1024
	N=`expr $N + 1`
done
