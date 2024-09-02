
# Liichess

A custom homebrew lichess client for the Nintendo Wii using the lichess API

## License

Liichess is licensed under the GPL3 license. Libwiigui also uses the GPL license and mbedtls, which is included, uses a dual Apache or GPL-2.0-or-later license. CURL uses the curl license, which is included separately. Rapidjson uses the MIT license, which is also included separately.

## Installation


If you do not want to deal with building the project yourself, there are prebuilt releases [here](https://github.com/WiiExpand/Liichess/releases/) or on the Open Shop Channel. Once you have downloaded the release, extract the zip file into the apps folder on your SD card and open the app from the homebrew channel. If you wish to build the project yourself, instructions are below.
    
## Signing In

By default, if Liichess is started without being signed in, you will be prompted to sign in with your lichess.org account using your username and password. If you wish to directly use your Personal API Access Token instead, Create a folder on your SD card's root called **LichessWii**, then create a file named **token.txt** inside of this folder. Place your Personal API Access Token in token.txt. Once you open Liichess, the token will be detected and you will not be prompted to sign in.

If you choose to sign in with your username and password instead, Liichess will automatically create a Personal API Access Token on your behalf.

Note that all communication performed by Liichess is directly with the lichess.org API and will never involve other sources.

## Build Instructions

These instructions assume you have devkitPro installed already --- if you don't, install it.

Clone the repository:

```bash
  git clone https://github.com/WiiExpand/Liichess.git
```

Go to the project directory:

```bash
  cd Liichess
```

There are only three dependencies that need to be installed. The rest of the dependencies are pre-packaged in the repository for convenience. Install the necessary dependencies:

```bash
  pacman -S ppc-freetype
  pacman -S ppc-libvorbisidec
  pacman -S ppc-libogg
```

Compile:

```bash
  make
```

Finally, after the compilation has finished you should be left with the resulting **liichess.dol**.


## Screenshots

*A game in play*
![App Screenshot](https://i.imgur.com/JBS7JmN.png)

*The main menu*
![App Screenshot](https://i.imgur.com/X2vkYn7.png)

*Creating a challenge*
![App Screenshot](https://i.imgur.com/zHz88z2.png)

*The sign-in screen*
![App Screenshot](https://i.imgur.com/GLPyqs4.png)


