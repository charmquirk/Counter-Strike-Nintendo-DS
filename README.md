
# CharmQuirk's Fork of Counter Strike NDS

I'm editing the code on my own. I am unable to build and test the code right now. There are no releases of this fork available yet. Contact me if you have any questions. Checkout the [main branch of the original repository](https://github.com/Fewnity/Counter-Strike-Nintendo-DS).

## CharmQuirk's TODO

I have a few changes I would like to make with my fork of this game. Each feature is at a different level of completion:

- Not started
- Started
- Partially complete
- Testing needed
- Complete
- Revised

### General Improvements

- [ ] Code cleanup. **Ongoing**
- [ ] Update the project to BlocksDS.

### Improve input handling

- [ ] Normalizing movement vectors, preventing moving faster horizontally. **Testing Needed**
- [ ] Move all movement code to the movement script. Take it OUT of the main script aka the main game loop. **Partially Complete**
- [ ] Add "double tap touch screen to jump" from Metroid Prime: Hunters DS (as requested).
- [ ] Add better input customization and re-add old control scheme.
- [ ] Add touch input aim acceleration curve.

![Image : Bottom Screen UI Concept](https://github.com/charmquirk/Counter-Strike-Nintendo-DS/blob/main/.external/UI%20Concepts/UI%20Concepts%20-%20Bottom%20Screen.png)

### Improve UI

- [ ] Add screen with a button to skip tutorial.
- [ ] Add a proper title screen.
- [ ] Add more icons for better readability.
>- Scoreboard: Trophy with a list
>- Settings: Gear
- [ ] Add an optional defuse/plant bomb ui button on touch screen. If added:
 	- It would allows users to quickly identify if they have the bomb no matter what weapon they had equipped
 	- it would allow for faster weapon switching when canceling defusing/planting
    - The progress bar can be on the touch screen.
 	- it is an easier input to toggle. Planting requires more strain hold "L", and holding "Select" to defuse is very inconvenient.
- [ ] Temporarily remove Jump ui button from the touch screen.
- [ ] Reorganize settings menu and add tabs. Controls manages input settings such as keyboard mode, multiplayer allows you to change your name, graphics allows you to change your crosshair transparency, etc.
- [ ] Reorganize the bottom screen. Put camo matching team color on bottom screen

### Improve Networking

- [ ] Optimize network code. **Started**
- [ ] Implement Rollback with player prediction.
