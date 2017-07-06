sudo apt-get -y -q update && sudo apt-get -y -q upgrade && sudo apt -y -q dist-upgrade
sudo apt-get -y -q install software-properties-common && sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get -y -q update && sudo apt-get -y -q upgrade && sudo apt -y -q dist-upgrade
sudo apt-get -y -q install gcc-4.9 g++-4.9 zlib1g-dev libsdl1.2-dev libjpeg-dev nasm tar libbz2-dev libgtk2.0-dev cmake mercurial libfluidsynth-dev libgl1-mesa-dev libssl-dev libglew-dev

mkdir buildclient
wget -nc http://zandronum.com/essentials/fmod/fmodapi42416linux64.tar.gz 
tar -xvzf fmodapi42416linux64.tar.gz

