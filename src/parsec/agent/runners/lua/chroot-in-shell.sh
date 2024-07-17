# steps:
# 1. create chroot environment
# 2. create directories
# 3. copy binaries we want
# 4. import dependencies
# 5. chroot in!

# arg1=path, arg2=wanted binaries separated by commas
# e.g. ./chroot-in-shell.sh /home/nicoli/Desktop/jail bash,touch,ls,rm

chr="$1" # creates chroot environment
mkdir -p $chr # creates necessary directories
mkdir -p $chr/{bin,lib.lib64} 
cd $chr


# wanted binaries
string="$2"
IFS=',' read -r -a wanted <<< "$string"

# # copies wanted binaries
for binary in ${wanted[@]}; do cp /bin/$binary $chr/bin; done

# # copies all dependencies
for binary in ${wanted[@]}; do
    list="$(ldd /bin/$binary | egrep -o '/lib.*\.[0-9]')"
    for i in $list; do cp --parents "$i" "${chr}"; done
done 


# enter chroot
/usr/sbin/chroot $chr /bin/bash
# sudo chroot $chr [#insert agent here]
