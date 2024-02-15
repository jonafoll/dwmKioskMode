# dwmKioskMode
Config files for running dwm in kiosk mode with surf browser

Modify the URL in the spawn function in dwm.c to change website

# Setup for Debian network install
https://www.debian.org/CD/netinst/

    apt install vim git surf xserver-xorg-core xinit xinput x11-xserver-utils build-essentials 

# DWM build:
    mkdir .config
    git clone https://git.suckless.org/dwm
    make clean install


# DWM configuration:
    git clone https://github.com/jonafoll/dwmKioskMode

    cd dwmKioskMode
    cp config.h ~/.config/dwm
    cd ~/.config/dwm

    rm -rf dwm.c
    cd ~/dwmKioskMode
    cp dwm.c ~/.config/dwm

    make clean install

# .xinitrc setup:
    cd dwmKioskMode
    cat .xinitrc.txt >> ~/.xinitrc

# startx
When you run startx, it should launch right into surf browser in kiosk mode.
