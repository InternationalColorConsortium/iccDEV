dockerDir = 'C:\Program Files\Docker\Docker\resources\bin';
setenv('PATH', [dockerDir pathsep getenv('PATH')]);
system('docker version')
run_local_qa