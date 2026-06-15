import random
import time
import tkinter as tk
from dataclasses import dataclass, field


WORLD_COLS = 80
WORLD_ROWS = 80
TARGET_PELLETS = 160
MAX_PELLETS = 240
MATCH_MS = 0
DEFAULT_AI_COUNT = 4
MAX_SNAKE_LEN = 512
RESPAWN_MS = 2_000
NORMAL_STEP_MS = 130
BOOST_STEP_MS = 75
MIN_BOOST_LEN = 7
BOOST_COST_MOVES = 4

LCD_W = 240
LCD_H = 320
SCALE = 2
HEADER_H = 48
BOARD_X = 15
BOARD_Y = 58
CELL_W = 14
CELL_H = 12
VIEW_COLS = 15
VIEW_ROWS = 20

DIR_UP = 0
DIR_DOWN = 1
DIR_LEFT = 2
DIR_RIGHT = 3

PELLET_NORMAL = 0
PELLET_CORPSE = 1
SKIN_CLASSIC = 0
SKIN_NAILOONG = 1

COLORS = {
    "bg": "#081018",
    "panel": "#101b25",
    "grid": "#172431",
    "text": "#e8f4ff",
    "muted": "#86a2b8",
    "p1_head": "#fff068",
    "p1_body": "#39d77a",
    "p1_shadow": "#1d7f49",
    "p2_head": "#ff7de8",
    "p2_body": "#41d9ff",
    "p2_shadow": "#1b7f99",
    "ai2_head": "#ff9f43",
    "ai2_body": "#ffca6b",
    "ai2_shadow": "#8a5524",
    "ai3_head": "#b98cff",
    "ai3_body": "#d1b3ff",
    "ai3_shadow": "#5c4280",
    "ai4_head": "#ff5b62",
    "ai4_body": "#ff8a91",
    "ai4_shadow": "#7d3035",
    "danger": "#ff5b62",
}

PELLET_COLORS = [
    "#ff5b62",
    "#ffd166",
    "#53e88b",
    "#41d9ff",
    "#b98cff",
    "#ff9f43",
]


@dataclass
class Pellet:
    x: int = 0
    y: int = 0
    active: bool = False
    value: int = 1
    color: int = 0
    kind: int = PELLET_NORMAL


@dataclass
class Snake:
    x: list[int] = field(default_factory=list)
    y: list[int] = field(default_factory=list)
    prev_x: list[int] = field(default_factory=list)
    prev_y: list[int] = field(default_factory=list)
    score: int = 0
    dir: int = DIR_RIGHT
    next_dir: int = DIR_RIGHT
    move_acc_ms: float = 0.0
    respawn_ms: float = 0.0
    alive: bool = True
    boost_on: bool = False
    boost_cost_counter: int = 0

    @property
    def length(self) -> int:
        return len(self.x)


class BattleGame:
    def __init__(
        self,
        seed: int | None = None,
        player_count: int = 2,
        ai_enabled: bool = False,
        ai_count: int = DEFAULT_AI_COUNT,
    ) -> None:
        self.rng = random.Random(seed if seed is not None else int(time.time()))
        self.player_count = player_count
        self.ai_enabled = ai_enabled
        self.ai_count = ai_count if ai_enabled else 0
        self.snakes: list[Snake] = []
        self.pellets = [Pellet() for _ in range(MAX_PELLETS)]
        self.elapsed_ms = 0.0
        self.match_over = False
        self.winner = 0
        self.last_events: list[str] = []
        self.reset()

    def reset(self) -> None:
        active_count = self._active_count()
        self.snakes = [Snake() for _ in range(active_count)]
        self.pellets = [Pellet() for _ in range(MAX_PELLETS)]
        self.elapsed_ms = 0.0
        self.match_over = False
        self.winner = 0
        self.last_events = []
        self._set_snake(0, 14, 40, DIR_RIGHT)
        for i in range(1, active_count):
            x = min(WORLD_COLS - 8, 20 + i * 9)
            y = 16 + ((i * 13) % (WORLD_ROWS - 24))
            self._set_snake(i, x, y, DIR_LEFT if i % 2 else DIR_RIGHT)
        self._maintain_pellets()

    def _active_count(self) -> int:
        if self.ai_enabled:
            return 1 + max(1, self.ai_count)
        return max(1, self.player_count)

    def set_player_count(self, player_count: int) -> None:
        self.player_count = 1 if player_count <= 1 else 2
        self.ai_enabled = False
        self.reset()

    def set_ai_mode(self) -> None:
        self.player_count = 1 + DEFAULT_AI_COUNT
        self.ai_enabled = True
        self.ai_count = DEFAULT_AI_COUNT
        self.reset()

    def _set_snake(self, player: int, head_x: int, head_y: int, direction: int) -> None:
        snake = self.snakes[player]
        snake.x = []
        snake.y = []
        snake.dir = direction
        snake.next_dir = direction
        snake.move_acc_ms = 0.0
        snake.respawn_ms = 0.0
        snake.alive = True
        snake.boost_on = False
        snake.boost_cost_counter = 0
        if player == 0:
            snake.score = snake.score if snake.score else 0
        else:
            snake.score = snake.score if snake.score else 0

        for i in range(6):
            if direction == DIR_RIGHT:
                snake.x.append(head_x - i)
                snake.y.append(head_y)
            elif direction == DIR_LEFT:
                snake.x.append(head_x + i)
                snake.y.append(head_y)
            elif direction == DIR_DOWN:
                snake.x.append(head_x)
                snake.y.append(head_y - i)
            else:
                snake.x.append(head_x)
                snake.y.append(head_y + i)
        snake.prev_x = snake.x.copy()
        snake.prev_y = snake.y.copy()

    @staticmethod
    def _is_reverse(a: int, b: int) -> bool:
        return (
            (a == DIR_UP and b == DIR_DOWN)
            or (a == DIR_DOWN and b == DIR_UP)
            or (a == DIR_LEFT and b == DIR_RIGHT)
            or (a == DIR_RIGHT and b == DIR_LEFT)
        )

    @staticmethod
    def _advance(direction: int, x: int, y: int) -> tuple[int, int]:
        if direction == DIR_UP:
            return x, y - 1
        if direction == DIR_DOWN:
            return x, y + 1
        if direction == DIR_LEFT:
            return x - 1, y
        return x + 1, y

    @staticmethod
    def _in_world(x: int, y: int) -> bool:
        return 0 <= x < WORLD_COLS and 0 <= y < WORLD_ROWS

    def _snake_has_cell(self, snake: Snake, x: int, y: int, limit: int | None = None) -> bool:
        if not snake.alive:
            return False
        limit = snake.length if limit is None else min(limit, snake.length)
        for i in range(limit):
            if snake.x[i] == x and snake.y[i] == y:
                return True
        return False

    def _pellet_at(self, x: int, y: int) -> int | None:
        for i, pellet in enumerate(self.pellets):
            if pellet.active and pellet.x == x and pellet.y == y:
                return i
        return None

    def _cell_free(self, x: int, y: int) -> bool:
        if self._pellet_at(x, y) is not None:
            return False
        return not any(
            self._snake_has_cell(snake, x, y)
            for snake in self.snakes
        )

    def _free_pellet_slot(self) -> int:
        for i, pellet in enumerate(self.pellets):
            if not pellet.active:
                return i
        return self.rng.randrange(MAX_PELLETS)

    def _place_pellet(self, x: int, y: int, kind: int = PELLET_NORMAL, value: int = 1) -> None:
        slot = self._free_pellet_slot()
        self.pellets[slot] = Pellet(
            x=x,
            y=y,
            active=True,
            value=value,
            color=self.rng.randrange(len(PELLET_COLORS)),
            kind=kind,
        )

    def _place_random_pellet(self) -> None:
        for _ in range(500):
            x = self.rng.randrange(WORLD_COLS)
            y = self.rng.randrange(WORLD_ROWS)
            if self._cell_free(x, y):
                self._place_pellet(x, y)
                return

    def _maintain_pellets(self) -> None:
        active = sum(1 for pellet in self.pellets if pellet.active)
        while active < TARGET_PELLETS:
            self._place_random_pellet()
            active += 1

    def set_dir(self, player: int, direction: int) -> None:
        snake = self.snakes[player]
        if snake.alive and not self._is_reverse(snake.dir, direction):
            snake.next_dir = direction

    def _drop_snake_pellets(self, player: int) -> None:
        snake = self.snakes[player]
        for i in range(0, snake.length, 2):
            self._place_pellet(snake.x[i], snake.y[i], PELLET_CORPSE, 2)

    def _kill(self, player: int) -> None:
        snake = self.snakes[player]
        if not snake.alive:
            return
        self._drop_snake_pellets(player)
        snake.alive = False
        snake.x = []
        snake.y = []
        snake.prev_x = []
        snake.prev_y = []
        snake.respawn_ms = RESPAWN_MS
        snake.move_acc_ms = 0.0
        snake.boost_on = False
        self.last_events.append(f"P{player + 1} down")

    def _respawn_safe(self, x: int, y: int) -> bool:
        for yy in range(max(0, y - 3), min(WORLD_ROWS, y + 4)):
            for xx in range(max(0, x - 3), min(WORLD_COLS, x + 4)):
                if not self._cell_free(xx, yy):
                    return False
        return True

    def _try_respawn(self, player: int) -> None:
        for _ in range(300):
            x = self.rng.randrange(4, WORLD_COLS - 4)
            y = self.rng.randrange(4, WORLD_ROWS - 4)
            if self._respawn_safe(x, y):
                self._set_snake(player, x, y, DIR_RIGHT if player == 0 else DIR_LEFT)
                self.last_events.append(f"P{player + 1} respawn")
                return

    def _hits_body(
        self,
        snake: Snake,
        x: int,
        y: int,
        moving: bool,
        growing: bool,
    ) -> bool:
        limit = snake.length
        if moving and not growing and limit > 0:
            limit -= 1
        return self._snake_has_cell(snake, x, y, limit)

    def _move_snake(self, player: int, nx: int, ny: int, pellet_index: int | None) -> None:
        snake = self.snakes[player]
        old_x = snake.x.copy()
        old_y = snake.y.copy()
        old_tail = old_x[-1], old_y[-1]
        new_len = snake.length

        if pellet_index is not None and self.pellets[pellet_index].active:
            pellet = self.pellets[pellet_index]
            new_len += pellet.value
            snake.score += pellet.value
            pellet.active = False
            self.last_events.append(f"P{player + 1} +{pellet.value}")

        if snake.boost_on and snake.length > MIN_BOOST_LEN:
            snake.boost_cost_counter += 1
            if snake.boost_cost_counter >= BOOST_COST_MOVES:
                snake.boost_cost_counter = 0
                if new_len > MIN_BOOST_LEN:
                    new_len -= 1
                    self._place_pellet(old_tail[0], old_tail[1], PELLET_NORMAL, 1)
        else:
            snake.boost_cost_counter = 0

        snake.prev_x = old_x
        snake.prev_y = old_y
        new_x = [nx]
        new_y = [ny]
        for i in range(1, new_len):
            src = i - 1
            if src < len(old_x):
                new_x.append(old_x[src])
                new_y.append(old_y[src])
            else:
                new_x.append(old_tail[0])
                new_y.append(old_tail[1])
        snake.x = new_x[:512]
        snake.y = new_y[:512]

    def _step(self, move: list[bool]) -> None:
        count = len(self.snakes)
        nx = [0 for _ in range(count)]
        ny = [0 for _ in range(count)]
        pellet = [None for _ in range(count)]
        growing = [False for _ in range(count)]
        dead = [False for _ in range(count)]

        for i, snake in enumerate(self.snakes):
            if not snake.alive:
                continue
            nx[i], ny[i] = snake.x[0], snake.y[0]
            if move[i]:
                snake.dir = snake.next_dir
                nx[i], ny[i] = self._advance(snake.dir, nx[i], ny[i])
                pellet[i] = self._pellet_at(nx[i], ny[i]) if self._in_world(nx[i], ny[i]) else None
                growing[i] = pellet[i] is not None

        for i, snake in enumerate(self.snakes):
            if move[i] and not self._in_world(nx[i], ny[i]):
                dead[i] = True

        for i in range(count):
            if not move[i] or dead[i]:
                continue
            for j, other in enumerate(self.snakes):
                if i == j:
                    continue
                if self._hits_body(other, nx[i], ny[i], move[j], growing[j]):
                    dead[i] = True
                    break

        for i in range(count):
            if not move[i] or dead[i] or not self.snakes[i].alive:
                continue
            for j in range(i + 1, count):
                if move[j] and not dead[j] and self.snakes[j].alive and nx[i] == nx[j] and ny[i] == ny[j]:
                    dead[i] = True
                    dead[j] = True

        for i in range(count):
            if dead[i]:
                self._kill(i)

        consumed_pellets: set[int] = set()
        for i in range(count):
            if move[i] and not dead[i] and self.snakes[i].alive:
                if pellet[i] in consumed_pellets:
                    pellet[i] = None
                elif pellet[i] is not None:
                    consumed_pellets.add(pellet[i])
                self._move_snake(i, nx[i], ny[i], pellet[i])

        self._maintain_pellets()

    def _ai_score_direction(self, ai_index: int, direction: int, target: tuple[int, int]) -> int:
        ai = self.snakes[ai_index]
        nx, ny = self._advance(direction, ai.x[0], ai.y[0])
        if not self._in_world(nx, ny):
            return -100_000

        for other_index, other in enumerate(self.snakes):
            if other_index == ai_index:
                continue
            if self._hits_body(other, nx, ny, True, False):
                return -80_000

        distance = abs(target[0] - nx) + abs(target[1] - ny)
        wall_space = min(nx, ny, WORLD_COLS - 1 - nx, WORLD_ROWS - 1 - ny)
        score = -distance * 10 + wall_space * 2

        for other_index, other in enumerate(self.snakes):
            if other_index == ai_index or not other.alive or other.length == 0:
                continue
            head_distance = abs(other.x[0] - nx) + abs(other.y[0] - ny)
            if head_distance < 4:
                score -= (4 - head_distance) * 18
        return score

    def _nearest_ai_target(self, ai_index: int) -> tuple[int, int]:
        ai = self.snakes[ai_index]
        best = None
        best_score = -100_000
        for pellet in self.pellets:
            if not pellet.active:
                continue
            dist = abs(pellet.x - ai.x[0]) + abs(pellet.y - ai.y[0])
            score = pellet.value * 18 - dist
            if score > best_score:
                best_score = score
                best = (pellet.x, pellet.y)

        if best is not None:
            return best
        return WORLD_COLS // 2, WORLD_ROWS // 2

    def _update_ai(self) -> None:
        if not self.ai_enabled or len(self.snakes) < 2:
            return

        for ai_index in range(1, len(self.snakes)):
            ai = self.snakes[ai_index]
            if not ai.alive or ai.length == 0:
                continue

            target = self._nearest_ai_target(ai_index)
            candidates = [DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT]
            candidates = [d for d in candidates if not self._is_reverse(ai.dir, d)]
            if not candidates:
                candidates = [ai.dir]

            best_dir = max(candidates, key=lambda d: self._ai_score_direction(ai_index, d, target))
            if self._ai_score_direction(ai_index, best_dir, target) > -50_000:
                ai.next_dir = best_dir

            dist = abs(target[0] - ai.x[0]) + abs(target[1] - ai.y[0])
            ai.boost_on = ai.length > 16 and dist > 9 and self.rng.randrange(5) != 0

    def update(self, dt_ms: float, boosts: list[bool]) -> None:
        self.last_events = []
        if self.match_over:
            return

        self.elapsed_ms += dt_ms
        if MATCH_MS and self.elapsed_ms >= MATCH_MS:
            self.elapsed_ms = MATCH_MS
            self.match_over = True
            if self.player_count == 1:
                self.winner = 1
            elif self.snakes[0].score > self.snakes[1].score:
                self.winner = 1
            elif self.snakes[1].score > self.snakes[0].score:
                self.winner = 2
            else:
                self.winner = 3
            return

        self._update_ai()

        for i, snake in enumerate(self.snakes):
            if not snake.alive:
                snake.respawn_ms -= dt_ms
                if snake.respawn_ms <= 0:
                    self._try_respawn(i)
                continue
            if not (self.ai_enabled and i > 0):
                snake.boost_on = boosts[i] and snake.length > MIN_BOOST_LEN
            snake.move_acc_ms += dt_ms

        while True:
            move = [False for _ in self.snakes]
            for i, snake in enumerate(self.snakes):
                if not snake.alive:
                    continue
                interval = BOOST_STEP_MS if snake.boost_on else NORMAL_STEP_MS
                if snake.move_acc_ms >= interval:
                    snake.move_acc_ms -= interval
                    move[i] = True
            if not any(move):
                break
            self._step(move)


class BattleSimApp:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("SnakePilot Battle Simulator")
        self.canvas = tk.Canvas(
            self.root,
            width=LCD_W * SCALE,
            height=LCD_H * SCALE,
            bg=COLORS["bg"],
            highlightthickness=0,
        )
        self.canvas.pack()
        self.game = BattleGame(player_count=2, ai_enabled=True)
        self.keys: set[str] = set()
        self.paused = False
        self.fps = 30
        self.last_time = time.perf_counter()
        self.fps_acc = 0.0
        self.fps_frames = 0
        self.fps_text = 0
        self.log_lines: list[str] = []
        self.viewport_x = 0.0
        self.viewport_y = 0.0
        self.player_skin = SKIN_CLASSIC

        self.root.bind("<KeyPress>", self.on_key_down)
        self.root.bind("<KeyRelease>", self.on_key_up)
        self.root.after(0, self.loop)

    def on_key_down(self, event: tk.Event) -> None:
        key = event.keysym
        self.keys.add(key)

        mapping = {
            "w": (0, DIR_UP),
            "W": (0, DIR_UP),
            "s": (0, DIR_DOWN),
            "S": (0, DIR_DOWN),
            "a": (0, DIR_LEFT),
            "A": (0, DIR_LEFT),
            "d": (0, DIR_RIGHT),
            "D": (0, DIR_RIGHT),
            "Up": (1, DIR_UP),
            "Down": (1, DIR_DOWN),
            "Left": (1, DIR_LEFT),
            "Right": (1, DIR_RIGHT),
        }
        if key in mapping:
            player, direction = mapping[key]
            if not (self.game.ai_enabled and player == 1):
                self.game.set_dir(player, direction)
        elif key == "space":
            self.paused = not self.paused
        elif key in ("r", "R"):
            self.game = BattleGame(
                player_count=self.game.player_count,
                ai_enabled=self.game.ai_enabled,
            )
            self.log_lines = []
        elif key in ("f", "F"):
            self.fps = 15 if self.fps == 30 else 30
        elif key == "1":
            self.game = BattleGame(player_count=1)
            self.log_lines = ["Solo mode"]
        elif key == "2":
            self.game = BattleGame(player_count=2, ai_enabled=True)
            self.log_lines = ["Solo AI mode"]
        elif key == "3":
            self.game = BattleGame(player_count=2)
            self.log_lines = ["Duo mode"]
        elif key in ("k", "K"):
            self.player_skin = SKIN_NAILOONG if self.player_skin == SKIN_CLASSIC else SKIN_CLASSIC
            self.log_lines = ["Nailoong skin" if self.player_skin == SKIN_NAILOONG else "Classic skin"]
        elif key == "4":
            self.player_skin = SKIN_CLASSIC
            self.log_lines = ["Classic skin"]
        elif key == "5":
            self.player_skin = SKIN_NAILOONG
            self.log_lines = ["Nailoong skin"]

    def on_key_up(self, event: tk.Event) -> None:
        self.keys.discard(event.keysym)

    def sx(self, x: float) -> float:
        return x * SCALE

    def sy(self, y: float) -> float:
        return y * SCALE

    def rect(self, x1: float, y1: float, x2: float, y2: float, **kwargs) -> int:
        return self.canvas.create_rectangle(
            self.sx(x1), self.sy(y1), self.sx(x2), self.sy(y2), **kwargs
        )

    def oval(self, x1: float, y1: float, x2: float, y2: float, **kwargs) -> int:
        return self.canvas.create_oval(
            self.sx(x1), self.sy(y1), self.sx(x2), self.sy(y2), **kwargs
        )

    def polygon(self, points: list[tuple[float, float]], **kwargs) -> int:
        scaled: list[float] = []
        for x, y in points:
            scaled.extend([self.sx(x), self.sy(y)])
        return self.canvas.create_polygon(*scaled, **kwargs)

    def text(self, x: float, y: float, **kwargs) -> int:
        if "font" not in kwargs:
            kwargs["font"] = ("Consolas", 8 * SCALE)
        return self.canvas.create_text(self.sx(x), self.sy(y), **kwargs)

    def _choose_viewport(self) -> None:
        if self.game.player_count == 1 or self.game.ai_enabled:
            snake_list = self.game.snakes[:1]
        else:
            snake_list = self.game.snakes[: self.game.player_count]

        alive_heads = [
            (snake.x[0], snake.y[0])
            for snake in snake_list
            if snake.alive and snake.length > 0
        ]
        if not alive_heads:
            return
        cx = sum(p[0] for p in alive_heads) / len(alive_heads)
        cy = sum(p[1] for p in alive_heads) / len(alive_heads)
        target_x = max(0.0, min(WORLD_COLS - VIEW_COLS, cx - VIEW_COLS / 2))
        target_y = max(0.0, min(WORLD_ROWS - VIEW_ROWS, cy - VIEW_ROWS / 2))
        self.viewport_x += (target_x - self.viewport_x) * 0.18
        self.viewport_y += (target_y - self.viewport_y) * 0.18

    def _world_to_screen(self, x: float, y: float, margin: float = 1.0) -> tuple[float, float] | None:
        vx = x - self.viewport_x
        vy = y - self.viewport_y
        if vx < -margin or vy < -margin or vx > VIEW_COLS + margin or vy > VIEW_ROWS + margin:
            return None
        return BOARD_X + vx * CELL_W, BOARD_Y + vy * CELL_H

    def _draw_header(self) -> None:
        p1 = self.game.snakes[0]
        elapsed_s = int(self.game.elapsed_ms) // 1000
        if self.game.ai_enabled:
            title = "SOLO AI"
        elif self.game.player_count == 1:
            title = "SOLO"
        else:
            title = "BATTLE"
        self.rect(0, 0, LCD_W, HEADER_H, fill=COLORS["panel"], outline="")
        self.text(10, 8, anchor="nw", text=title, fill=COLORS["text"], font=("Consolas", 9 * SCALE, "bold"))
        self.text(10, 25, anchor="nw", text=f"P1 {p1.score:03d}", fill=COLORS["p1_head"])
        if self.player_skin == SKIN_NAILOONG:
            stage_label = ["BABY", "KID", "TEEN", "ADULT"][self._nailoong_stage(p1.length)]
            self.text(62, 8, anchor="nw", text=stage_label, fill="#ffeeb0")
        if self.game.ai_enabled:
            ai_total = sum(s.score for s in self.game.snakes[1:])
            self.text(76, 25, anchor="nw", text=f"AIx{len(self.game.snakes) - 1}", fill=COLORS["p2_head"])
            self.text(126, 25, anchor="nw", text=f"{ai_total:03d}", fill=COLORS["muted"])
        elif self.game.player_count > 1:
            p2 = self.game.snakes[1]
            self.text(76, 25, anchor="nw", text=f"P2 {p2.score:03d}", fill=COLORS["p2_head"])
        else:
            self.text(76, 25, anchor="nw", text=f"LEN {p1.length:03d}", fill=COLORS["muted"])
        self.text(176, 8, anchor="nw", text=f"{elapsed_s:03d}s", fill=COLORS["text"], font=("Consolas", 9 * SCALE, "bold"))
        self.text(176, 25, anchor="nw", text=f"{self.fps}fps", fill=COLORS["muted"])

        bar_x = 88
        bar_y = 12
        bar_w = 74
        self.rect(bar_x, bar_y, bar_x + bar_w, bar_y + 5, fill="#223241", outline="")
        len_pct = min(1.0, p1.length / 80.0)
        self.rect(bar_x, bar_y, bar_x + bar_w * len_pct, bar_y + 5, fill="#53e88b", outline="")

    def _draw_board(self) -> None:
        self.rect(0, HEADER_H, LCD_W, LCD_H, fill=COLORS["bg"], outline="")
        self.rect(
            BOARD_X - 2,
            BOARD_Y - 2,
            BOARD_X + VIEW_COLS * CELL_W + 1,
            BOARD_Y + VIEW_ROWS * CELL_H + 1,
            fill="#07131d",
            outline="#24465c",
        )
        for i in range(28):
            x = BOARD_X + (i * 37 % (VIEW_COLS * CELL_W))
            y = BOARD_Y + (i * 53 % (VIEW_ROWS * CELL_H))
            self.oval(x - 0.8, y - 0.8, x + 0.8, y + 0.8, fill="#183144", outline="")

    def _draw_pellets(self) -> None:
        for pellet in self.game.pellets:
            if not pellet.active:
                continue
            pos = self._world_to_screen(pellet.x + 0.5, pellet.y + 0.5)
            if pos is None:
                continue
            cx, cy = pos
            radius = 4.1 if pellet.kind == PELLET_CORPSE else 3.0
            color = "#fff3a3" if pellet.kind == PELLET_CORPSE else PELLET_COLORS[pellet.color]
            self.oval(cx - radius, cy - radius, cx + radius, cy + radius, fill=color, outline="")
            self.oval(cx - 1.1, cy - 1.1, cx + 1.1, cy + 1.1, fill="#ffffff", outline="")

    def _segment_pos(self, snake: Snake, index: int) -> tuple[float, float]:
        interval = BOOST_STEP_MS if snake.boost_on else NORMAL_STEP_MS
        alpha = min(1.0, snake.move_acc_ms / interval) if snake.alive else 0.0
        if index < len(snake.prev_x):
            px, py = snake.prev_x[index], snake.prev_y[index]
        else:
            px, py = snake.x[index], snake.y[index]
        return px + (snake.x[index] - px) * alpha, py + (snake.y[index] - py) * alpha

    def _snake_paths(self, snake: Snake, margin: float = 2.5) -> list[list[float]]:
        paths: list[list[float]] = []
        current_path: list[float] = []
        last_pos: tuple[float, float] | None = None

        for i in range(snake.length):
            wx, wy = self._segment_pos(snake, i)
            pos = self._world_to_screen(wx + 0.5, wy + 0.5, margin=margin)
            if pos is None:
                if len(current_path) >= 4:
                    paths.append(current_path)
                current_path = []
                last_pos = None
                continue
            if last_pos is not None:
                dx = pos[0] - last_pos[0]
                dy = pos[1] - last_pos[1]
                if dx * dx + dy * dy > (CELL_W * 2.2) * (CELL_W * 2.2):
                    if len(current_path) >= 4:
                        paths.append(current_path)
                    current_path = []
            current_path.extend([self.sx(pos[0]), self.sy(pos[1])])
            last_pos = pos

        if len(current_path) >= 4:
            paths.append(current_path)
        return paths

    @staticmethod
    def _nailoong_stage(length: int) -> int:
        if length < 12:
            return 0
        if length < 24:
            return 1
        if length < 45:
            return 2
        return 3

    def _draw_nailoong_snake(self, snake: Snake) -> None:
        stage = self._nailoong_stage(snake.length)
        body_colors = ["#ffe06a", "#ffd25a", "#ffc247", "#ffad3d"]
        shadow_colors = ["#a66c1e", "#a8691b", "#9b5a1a", "#874817"]
        belly_colors = ["#fff8d8", "#fff2c8", "#ffe7b6", "#ffdca5"]
        stage_names = ["BABY", "KID", "TEEN", "ADULT"]
        horn_color = "#fff0b6"
        blush_color = "#ff9aac"
        eye_color = "#43c66f"
        body_color = body_colors[stage]
        shadow_color = shadow_colors[stage]
        belly_color = belly_colors[stage]
        body_width = [10, 12, 14, 16][stage]
        belly_width = [2, 3, 4, 5][stage]
        head_radius = [8.4, 9.4, 10.4, 11.4][stage]
        horn_size = [2.0, 3.0, 4.4, 5.6][stage]
        cheek_size = [3.2, 2.8, 2.2, 1.7][stage]
        eye_scale = [1.22, 1.08, 0.96, 0.86][stage]

        for line_points in self._snake_paths(snake, margin=2.5):
            self.canvas.create_line(
                *line_points,
                fill=shadow_color,
                width=(body_width + 5) * SCALE,
                smooth=True,
                capstyle=tk.ROUND,
                joinstyle=tk.ROUND,
            )
            self.canvas.create_line(
                *line_points,
                fill=body_color,
                width=body_width * SCALE,
                smooth=True,
                capstyle=tk.ROUND,
                joinstyle=tk.ROUND,
            )
            self.canvas.create_line(
                *line_points,
                fill=belly_color,
                width=belly_width * SCALE,
                smooth=True,
                capstyle=tk.ROUND,
                joinstyle=tk.ROUND,
            )

        for i in range(snake.length - 1, -1, -1):
            wx, wy = self._segment_pos(snake, i)
            pos = self._world_to_screen(wx + 0.5, wy + 0.5)
            if pos is None:
                continue
            cx, cy = pos
            t = 1.0 - min(i, 18) / 26.0
            radius = head_radius if i == 0 else (body_width * 0.38 + t * 0.9)
            if i != 0 and i % max(3, 6 - stage) == 0 and stage >= 1:
                fin_h = 2.0 + stage * 1.0
                fin_w = 1.2 + stage * 0.4
                self.polygon([(cx, cy - radius - fin_h),
                              (cx - fin_w, cy - radius + 1.0),
                              (cx + fin_w, cy - radius + 1.0)],
                             fill="#ff7d31", outline="")
            self.oval(cx - radius - 0.8, cy - radius + 1.0,
                      cx + radius + 0.8, cy + radius + 1.0,
                      fill=shadow_color, outline="")
            self.oval(cx - radius, cy - radius, cx + radius, cy + radius,
                      fill=body_color, outline="")
            if i != 0 and i % 3 == 0:
                self.oval(cx - belly_width * 0.7, cy + 0.5,
                          cx + belly_width * 0.7, cy + 2.6 + stage * 0.6,
                          fill=belly_color, outline="")

        hx, hy = self._segment_pos(snake, 0)
        head_pos = self._world_to_screen(hx + 0.5, hy + 0.5)
        if head_pos is None:
            return
        cx, cy = head_pos
        hr = head_radius
        horn = horn_size

        self.oval(cx - hr, cy - hr, cx + hr, cy + hr, fill=body_color, outline="")
        if stage == 0:
            self.oval(cx - 5.2, cy - hr - 1.2, cx - 2.8, cy - hr + 1.5,
                      fill=horn_color, outline="")
            self.oval(cx + 2.8, cy - hr - 1.2, cx + 5.2, cy - hr + 1.5,
                      fill=horn_color, outline="")
        else:
            self.polygon([(cx - 5.5, cy - hr + 1.0),
                          (cx - 2.9, cy - hr - horn),
                          (cx - 0.4, cy - hr + 1.0)],
                         fill=horn_color, outline="")
            self.polygon([(cx + 0.4, cy - hr + 1.0),
                          (cx + 2.9, cy - hr - horn),
                          (cx + 5.5, cy - hr + 1.0)],
                         fill=horn_color, outline="")
        if stage >= 2:
            self.oval(cx - hr - 2.8, cy + 0.8, cx - hr + 1.8, cy + 5.4,
                      fill=body_color, outline=shadow_color)
            self.oval(cx + hr - 1.8, cy + 0.8, cx + hr + 2.8, cy + 5.4,
                      fill=body_color, outline=shadow_color)
        if stage == 3:
            self.polygon([(cx, cy - hr - horn - 3.0),
                          (cx - 2.0, cy - hr - 0.8),
                          (cx + 2.0, cy - hr - 0.8)],
                         fill="#ff7d31", outline="")

        eye_w = 2.6 * eye_scale
        eye_h = 3.1 * eye_scale
        self.oval(cx - 4.1 - eye_w / 2, cy - 2.1 - eye_h / 2,
                  cx - 4.1 + eye_w / 2, cy - 2.1 + eye_h / 2,
                  fill=eye_color, outline="")
        self.oval(cx + 4.1 - eye_w / 2, cy - 2.1 - eye_h / 2,
                  cx + 4.1 + eye_w / 2, cy - 2.1 + eye_h / 2,
                  fill=eye_color, outline="")
        self.oval(cx - 4.1 - 0.7, cy - 2.1 - 0.9,
                  cx - 4.1 + 0.7, cy - 2.1 + 0.9,
                  fill="#10251b", outline="")
        self.oval(cx + 4.1 - 0.7, cy - 2.1 - 0.9,
                  cx + 4.1 + 0.7, cy - 2.1 + 0.9,
                  fill="#10251b", outline="")
        self.oval(cx - hr + 1.6, cy + 2.3,
                  cx - hr + 1.6 + cheek_size, cy + 2.3 + cheek_size,
                  fill=blush_color, outline="")
        self.oval(cx + hr - 1.6 - cheek_size, cy + 2.3,
                  cx + hr - 1.6, cy + 2.3 + cheek_size,
                  fill=blush_color, outline="")
        self.oval(cx - 2.0 - stage * 0.3, cy + 2.0,
                  cx + 2.0 + stage * 0.3, cy + 5.0 + stage * 0.6,
                  fill=belly_color, outline="")
        self.text(cx, cy + hr + 5.5, text=stage_names[stage],
                  fill="#ffeeb0", font=("Consolas", 5 * SCALE, "bold"))

    def _draw_snake(self, snake: Snake, player: int) -> None:
        if not snake.alive:
            return
        if player == 0 and self.player_skin == SKIN_NAILOONG:
            self._draw_nailoong_snake(snake)
            return

        palettes = [
            ("p1_head", "p1_body", "p1_shadow"),
            ("p2_head", "p2_body", "p2_shadow"),
            ("ai2_head", "ai2_body", "ai2_shadow"),
            ("ai3_head", "ai3_body", "ai3_shadow"),
            ("ai4_head", "ai4_body", "ai4_shadow"),
        ]
        head_key, body_key, shadow_key = palettes[player % len(palettes)]
        head_color = COLORS[head_key]
        body_color = COLORS[body_key]
        shadow_color = COLORS[shadow_key]

        for line_points in self._snake_paths(snake, margin=2.5):
            self.canvas.create_line(
                *line_points,
                fill=shadow_color,
                width=13 * SCALE,
                smooth=True,
                capstyle=tk.ROUND,
                joinstyle=tk.ROUND,
            )
            self.canvas.create_line(
                *line_points,
                fill=body_color,
                width=9 * SCALE,
                smooth=True,
                capstyle=tk.ROUND,
                joinstyle=tk.ROUND,
            )

        for i in range(snake.length - 1, -1, -1):
            wx, wy = self._segment_pos(snake, i)
            pos = self._world_to_screen(wx + 0.5, wy + 0.5)
            if pos is None:
                continue
            cx, cy = pos
            t = 1.0 - min(i, 18) / 26.0
            radius = 5.5 if i == 0 else 4.3 + t * 0.8
            self.oval(cx - radius - 0.8, cy - radius + 1.0, cx + radius + 0.8, cy + radius + 1.0, fill=shadow_color, outline="")
            self.oval(cx - radius, cy - radius, cx + radius, cy + radius, fill=head_color if i == 0 else body_color, outline="")
            if i == 0:
                self.oval(cx - 2.5, cy - 2.3, cx - 0.6, cy - 0.4, fill="#091016", outline="")
                self.oval(cx + 0.8, cy - 2.3, cx + 2.7, cy - 0.4, fill="#091016", outline="")

    def _draw_footer(self) -> None:
        boost1 = "BOOST" if self.game.snakes[0].boost_on else "     "
        boost2 = "BOOST" if len(self.game.snakes) > 1 and self.game.snakes[1].boost_on else "     "
        if self.game.player_count == 1:
            line = f"WASD {boost1} | K skin"
        elif self.game.ai_enabled:
            line = f"WASD {boost1} | AIx{len(self.game.snakes) - 1} | K skin"
        else:
            line = f"WASD {boost1} | ARROWS {boost2} | K skin"
        self.text(12, 304, anchor="nw", text=line, fill=COLORS["muted"])
        if self.paused:
            self.text(120, 159, text="PAUSED", fill=COLORS["text"], font=("Consolas", 14 * SCALE, "bold"))
        if self.game.match_over:
            if self.game.player_count == 1:
                msg = f"SCORE {self.game.snakes[0].score}"
            elif self.game.winner == 1:
                msg = "P1 WINS"
            elif self.game.winner == 2:
                msg = "AI WINS" if self.game.ai_enabled else "P2 WINS"
            else:
                msg = "DRAW"
            self.rect(45, 132, 195, 188, fill="#101b25", outline="#53e88b")
            self.text(120, 146, text=msg, fill=COLORS["text"], font=("Consolas", 13 * SCALE, "bold"))
            self.text(120, 170, text="R restart", fill=COLORS["muted"])

    def render(self) -> None:
        self.canvas.delete("all")
        self._choose_viewport()
        self._draw_header()
        self._draw_board()
        self._draw_pellets()
        for player in range(len(self.game.snakes) - 1, -1, -1):
            self._draw_snake(self.game.snakes[player], player)
        self._draw_footer()

    def loop(self) -> None:
        now = time.perf_counter()
        dt_ms = (now - self.last_time) * 1000.0
        self.last_time = now
        dt_ms = min(dt_ms, 80.0)

        if not self.paused:
            boosts = [
                "Shift_L" in self.keys or "Shift_R" in self.keys,
                "Return" in self.keys,
            ]
            self.game.update(dt_ms, boosts)
            if self.game.last_events:
                self.log_lines = (self.game.last_events + self.log_lines)[:4]

        self.fps_acc += dt_ms
        self.fps_frames += 1
        if self.fps_acc >= 500:
            self.fps_text = round(self.fps_frames * 1000 / self.fps_acc)
            self.fps_acc = 0.0
            self.fps_frames = 0

        self.render()
        self.root.after(int(1000 / self.fps), self.loop)

    def run(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    BattleSimApp().run()
