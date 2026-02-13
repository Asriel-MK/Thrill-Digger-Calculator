# Thrill Digger Calculator

Howdy! Thanks for checking out this project.

If you've ever played the **Thrill Digger** mini-game in *The Legend of Zelda: Skyward Sword* and felt like the game was just guessing and luck... well, you're mostly right (lmao) But there is some logic to it, and that's why I built this tool.

Someone (probably): "*hey but there is already a Thrill Digger Assistant made by Josh Scotland, why would i need this one*"

R: That's an excellent question!, first of all, if you haven't used that tool, i encourage you to use it as well, it is really well made by it's creator and you can find it here "https://www.joshscotland.com/thrill-digger-assistant/", the main reason why i decided to make my own tool, it's because the one that already exists has limitation to the computing power when caluclating multiple spots, calculation might need a few seconds to process, and after a bit, if it's too hard, the webpage might give up (because of how many calculations it does), i also wanted to learn myself about the logic behind how to calculate the probability of the spots. 

Regardless, this is a quite simple calculator app that helps you play the game smarter. It does the math for you so you can grab as many rupees as possible without blowing up (most of the time that is)

## ⚠️ Important Note
This calculator is specifically designed for the **Expert Difficulty** (the hardest one with the 5x8 grid). It assumes there are **8 Bombs** and **8 Rupoors** hiding on the board.

It won't work correctly for the Beginner or Intermediate (Please use Josh Scotland's tool for those)

## How to Use It

1.  **Open the Calculator**: Launch `ThrillDiggerCalculator.exe`.
2.  **Start Digging**: In the actual game, dig up a safe spot (usually a corner is a good guess!).
3.  **Update the Calculator**:
    *   Look at what rupee you found (Green, Blue, Red, Silver, Gold etc.).
    *   Find the matching square in the calculator and select that rupee from the dropdown menu.
4.  **Check the Odds**:
    *   The calculator will instantly update the numbers on the hidden squares.
    *   **Green (0%)**: Safe! Dig here next.
    *   **Red (100%)**: Danger! Do not dig here.
    *   **Yellow/Orange**: Proceed with caution. The percentage shows the chance of that spot being a Bomb or Rupoor.

## Getting the App
If you just want to use the tool, you can grab the latest `ThrillDiggerCalculator.exe` from the releases page (if available) or compile it yourself if you're tech-savvy.

### For Developers (How to Build)
If you want to look at the code or build it yourself:

*   **Easy Way (Windows)**: Just double-click `build.bat`. It requires the Visual Studio C++ compiler installed.
*   **Standard Way**: Use CMake (standard build commands apply).

Also i left a ungodly amount of comments through the files so if you want to modify or understand anything, you can!
---

*Enjoy the rupees! If you find this helpful, feel free to share it with other struggling heroes.*

*Made by: Fernando Núñez (Asriel-MK)*
