# ESP32-S3-GameBoy
# GameBoy
 
A tiny handheld game console I built on an ESP32-S3 while learning embedded C. It has a 128x64 OLED screen, three buttons and a buzzer, and it comes with six games and a menu to pick between them.
 
I started with one simple game to learn how the display and buttons work, and it kind of grew from there.

<img width="579" height="693" alt="image1" src="https://github.com/user-attachments/assets/56d6638b-7410-4e54-8c08-bcf70740736d" />
<img width="770" height="377" alt="image2" src="https://github.com/user-attachments/assets/5a97025e-cf46-4ec6-812a-889d26695091" />
<img width="542" height="619" alt="image3" src="https://github.com/user-attachments/assets/33639c85-fe61-4d7f-b726-ed3d2b92b438" />
<img width="631" height="703" alt="image4" src="https://github.com/user-attachments/assets/daf83ea0-4449-4e57-a737-10a0545ab4e7" />
 
## Games
 
- **Rocks**: shoot the rocks falling on you
- **Bounce**: breakout, smash all the bricks
- **Snake**: the classic one
- **Racing**: dodge traffic on a 3-lane road
- **Shooter**: Space Invaders style
- **Doom**: a small 3D raycaster with monsters (my favorite one)
High scores are saved in flash, so they stay there after you turn it off.

https://github.com/user-attachments/assets/b91bfb63-68c4-4e89-b1ed-832d0aed1aa1
 
## Hardware
 
- ESP32-S3 DevKit
- SSD1306 OLED 128x64 (I2C)
- 3 push buttons
- Passive buzzer
| Part | GPIO |
|---|---|
| OLED SDA | 8 |
| OLED SCL | 9 |
| Left button | 39 |
| Fire button | 41 |
| Right button | 21 |
| Buzzer | 14 |
 
Buttons go between the GPIO and GND (internal pull-ups are used). The buzzer goes between GPIO 14 and GND. All pins are in `main/pins.h` if you want to change them.
 
You can also run the whole thing in [Wokwi]([https://wokwi.com](https://wokwi.com/projects/479699689277648897)) without any hardware, which is how I built most of it.
 
## Controls
 
- **Menu:** left/right to choose a game, fire to start
- **In game:** hold left + right for a second to go back to the menu
- **Game over:** fire to play again, left or right for the menu
Most games use left/right to move and fire to shoot. A few work differently:
 
- **Snake:** left/right turn the snake, holding fire makes it faster
- **Racing:** holding fire gives you nitro
- **Doom:** left/right to turn, tap fire to shoot, hold fire to walk
## Build
 
You need ESP-IDF 5.x.
 
```
idf.py set-target esp32s3
idf.py build flash monitor
```
 
Or open the project folder in VS Code with the ESP-IDF extension and press build.
 
## How it's organized
 
Each game lives in its own `game_*.c` file and only has to implement three functions: `reset`, `step` and `score`. The menu in `main.c` doesn't care which game is running. To add a new game, create a file, register it in `main.c` and add it to `CMakeLists.txt`.
 
The display driver, input, sound and high score code are all written by hand, without any external libraries.
 
