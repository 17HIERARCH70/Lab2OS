файлы созданы через 
echo -ne '\x01\x01\x01\x01' > file1.bin  # Contains the some IP in big endian
echo -ne '\x01\x08\xA8\xC0' > file2.bin  # Contains the IP 192.168.8.1 in little endian
echo "no address here" > file3.txt
При поиске 192.168.8.1 должен выйти только file2.bin
./lab1psiN3245 -P ./plugin --ipv4-addr-bin 192.168.8.1 