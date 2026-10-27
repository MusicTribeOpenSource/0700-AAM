if [[ "$1" == "isMac" ]]
then
	echo "Build Kick Software for Mac OS"
	cd ./Source/Application
	cmake ./
	make -j16
	chmod +x apps/kick/Kick_artefacts/Kick.app/Contents/MacOS/Kick
	cd ../../
fi
if [[ "$1"  == "isLinux" ]]
then
	echo "Build Kick Software for Linux"
	sudo bash ./Linux/azure_build.sh Kick

	sudo mkdir Linux/buildroot/output/images/fw_files
	sudo mkdir Linux/buildroot/output/images/fwUpgrade_files

	sudo cp -f Linux/buildroot/output/images/*.dtb  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/Image.gz  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/imx8-boot-sd.bin  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/Kick_apps.ext4.gz  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/Kick_apps_debug.ext4.gz  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/sdcard_linux.img  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/rootfs.ext4.gz  Linux/buildroot/output/images/fw_files/
	sudo cp -f Linux/buildroot/output/images/EKick*  Linux/buildroot/output/images/fwUpgrade_files/
fi
if [[ "$1"  == "isApp" ]]
then
	echo "Build Kick Application for Linux"
	cd ./SDK
TAR_FILE="aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz"
EXTRACTED_DIR="/home/aarch64-buildroot-linux-gnu_sdk-buildroot"  # The expected extracted directory
	if [ -d "$EXTRACTED_DIR" ]; then
		echo "already extracted"
	else
		sudo tar -xvzf aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz -C /home/
	fi
	cd -
	sudo cmake -DCPM_SOURCE_CACHE="/home/3rdPartycache" -DCMAKE_BUILD_TYPE=Debug -DKICK_ENABLE_HARDWARE=ON -DCMAKE_TOOLCHAIN_FILE=/home/aarch64-buildroot-linux-gnu_sdk-buildroot/share/buildroot/toolchainfile.cmake -DCMAKE_INSTALL_PREFIX=./Build/  ./
	sudo make -j8
	sudo cmake -DCPM_SOURCE_CACHE="/home/3rdPartycache" -DCMAKE_BUILD_TYPE=Release -DKICK_ENABLE_HARDWARE=ON -DCMAKE_TOOLCHAIN_FILE=/home/aarch64-buildroot-linux-gnu_sdk-buildroot/share/buildroot/toolchainfile.cmake -DCMAKE_INSTALL_PREFIX=./Build/  ./
	sudo make -j8
fi
