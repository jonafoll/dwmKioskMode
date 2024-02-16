# dwmKioskMode
Config files for running dwm in kiosk mode with surf browser

Modify the URL in the spawn function in dwm.c to change website

# Setup for Debian network install
https://www.debian.org/CD/netinst/

    apt install vim git surf xserver-xorg-core xinit xinput x11-xserver-utils build-essentials libx11-dev libxinerama-dev libxft-dev 

# DWM build:
    mkdir .config
    cd .config
    git clone https://git.suckless.org/dwm
    make clean install


# DWM configuration:
    cd ~/
    git clone https://github.com/jonafoll/dwmKioskMode

    cd dwmKioskMode
    cp config.h ~/.config/dwm

    cd ~/.config/dwm
    make clean install

# .xinitrc setup:
    cd ~/dwmKioskMode
    cat .xinitrc.txt >> ~/.xinitrc

# startx
When you run startx, it should launch right into surf browser in kiosk mode.
