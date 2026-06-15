# SnakePilot Battle Simulator

This folder contains the PC-side visual debugger for the standalone Battle mode.
It uses Python's built-in Tkinter module, so no third-party package is required.

## Run

From the repository root:

```powershell
python Sim\battle_sim.py
```

The window renders a 240 x 320 LCD preview at 2x scale. It starts in solo
AI mode by default, with 1 player and 4 AI snakes.

## Controls

| Key | Action |
| --- | --- |
| W/A/S/D | Move P1 |
| Arrow keys | Move P2 in duo mode |
| Left/Right Shift | P1 boost |
| Enter | P2 boost in duo mode |
| Space | Pause/resume |
| F | Toggle 15/30 fps render pacing |
| R | Restart match |
| 1 | Switch to solo preview |
| 2 | Switch to solo AI mode, 1 player + 4 AI |
| 3 | Switch to duo battle |
| K | Toggle player skin |
| 4 | Use classic skin |
| 5 | Use Nailoong-inspired dragon skin |

## Current Rules

- The world is 80 x 80 cells.
- The LCD preview shows a 15 x 20 viewport.
- There is no time limit.
- The arena keeps at least 160 normal pellets.
- Eating normal pellets gives 1 point and grows by 1.
- Hitting another snake body or the world edge kills the snake.
- Overlapping your own body is allowed.
- Dead snakes drop corpse pellets worth 2 points each.
- Dead snakes respawn after 2 seconds at a safe location.
- Boosting moves faster, but every 4 boosted moves consumes 1 body segment and drops a normal pellet.
- Solo AI mode uses 4 simple bots that seek valuable pellets and avoid other snakes' bodies.

## Skins

- Classic: the original bright snake style.
- Nailoong-inspired dragon: an original procedural skin inspired by the yellow
  cartoon dragon look, with baby, juvenile, young adult, and adult visual states
  based on snake length. The current thresholds are baby `<12`, kid `<24`,
  teen `<45`, and adult `>=45`.

## Integration Notes

The simulator currently mirrors the rules in `User/Battle/battle_core.c`.
Keep rule changes in both places until the desktop simulator is switched to call
the C core directly.
