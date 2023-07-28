#!/usr/bin/python3

from enum import Enum
import copy
import time

# Only show moves, not individual solutions
only_moves = False

# Put correct sequence of keypresses into path.txt

class SquareState(Enum):
    GOOD = 0
    EMPTY = 1
    AVOID = 2
    LEVITATE = 3
    TRIGGER_1 = 4
    TRIGGER_2 = 5
    TRIGGERED_1 = 6
    TRIGGERED_2 = 7
    HIDDEN_1 = 8
    HIDDEN_2 = 9
    START = 10
    END = 11
    EPHEMERAL = 12

mapping = {
    '@': SquareState.GOOD,
    ' ': SquareState.EMPTY,
    'x': SquareState.AVOID,
    'L': SquareState.LEVITATE,
    '1': SquareState.TRIGGER_1,
    '2': SquareState.TRIGGER_2,
    'A': SquareState.TRIGGERED_1,
    'B': SquareState.TRIGGERED_2,
    'a': SquareState.HIDDEN_1,
    'b': SquareState.HIDDEN_2,
    's': SquareState.START,
    'e': SquareState.END,
    '#': SquareState.EPHEMERAL
}

def inv_map(ss):
    for i, s in mapping.items():
        if ss == s:
            return i
        

def index_of_2d(a, e):
    for i, l in enumerate(a):
        for j, f in enumerate(l):
            if f == e:
                return [j,i]
    return [-1,-1]

dim = 8

class MazeState:
    def __init__(self, s: str):
        self.squares = [ [mapping[sq] for sq in l] for l in s.split('\n') ]
        self.player = index_of_2d(self.squares, SquareState.START)
        self.history = ""
        self.won = False
        self.visited = 0

    def clone(self):
        return copy.deepcopy(self)

    def __hash__(self):
        return hash(str(self.squares)) ^ hash(tuple(self.player)) ^ hash(self.visited)

    def square_at(self, p):
        return self.squares[p[1]][p[0]]

    def set_square(self, p, e):
        self.squares[p[1]][p[0]] = e

    def handle_ephemeral(self):
        if self.square_at(self.player) == SquareState.EPHEMERAL:
            self.set_square(self.player, SquareState.EMPTY)

    def toggle_1(self):
        for l in self.squares:
            for j, s in enumerate(l):
                if s == SquareState.TRIGGERED_1:
                    l[j] = SquareState.HIDDEN_1
                elif s == SquareState.HIDDEN_1:
                    l[j] = SquareState.TRIGGERED_1

    def toggle_2(self):
        for l in self.squares:
            for j, s in enumerate(l):
                if s == SquareState.TRIGGERED_2:
                    l[j] = SquareState.HIDDEN_2
                elif s == SquareState.HIDDEN_2:
                    l[j] = SquareState.TRIGGERED_2

    def handle_triggers(self):
        sq = self.square_at(self.player)
        if sq in [ SquareState.AVOID, SquareState.START, SquareState.EMPTY, SquareState.HIDDEN_1, SquareState.HIDDEN_2 ]:
            return False

        if sq == SquareState.TRIGGER_1:
            self.toggle_1()
        elif sq == SquareState.TRIGGER_2:
            self.toggle_2()

        if sq == SquareState.END:
            self.won = True

        self.set_visited();

        return True

    def set_visited(self):
        p = self.player
        if self.square_at(p) is SquareState.EPHEMERAL:
            return
        self.visited = self.visited | (1 << (p[1] * dim + p[0]))

    def w(self):
        if self.player[1] >= dim - 1:
            return False

        self.handle_ephemeral()
        self.player[1] += 1

        self.history += 'w'

        return self.handle_triggers()

    def a(self):
        if self.player[0] >= dim - 1:
            return False
        self.handle_ephemeral()
        self.player[0] += 1

        self.history += 'a'

        return self.handle_triggers()

    def s(self):
        if self.player[1] == 0:
            return False
        self.handle_ephemeral()
        self.player[1] -= 1

        self.history += 's'

        return self.handle_triggers()

    def d(self):
        if self.player[0] == 0:
            return False
        self.handle_ephemeral()
        self.player[0] -= 1

        self.history += 'd'

        return self.handle_triggers()

    def to_str(self):
        s = '\n'.join([''.join(map(inv_map, l)) for l in self.squares])
        p = self.player
        i = p[1] * (dim + 1) + p[0]

        return s[:i] + 'p' + s[i+1:]

    def __eq__(self, other):
        for i, l in enumerate(self.squares):
            for j, s in enumerate(l):
                if self.squares[j][i] != other.squares[j][i]:
                    return False
        
        if self.player[0] != other.player[0] or self.player[1] != other.player[1] or self.visited != other.visited:
            return False
        return True


mazes = open("mazes.h", 'r').read().replace('\\','').replace(',','').replace('n','').split('\n\n')
mazes = map(lambda s: s.strip().replace('"',''), mazes)

def solve_instance(s, maze_i):
    visited = set()

    showed_win = False 

    seen = set()
    seen.add(MazeState(s))

    wins = set()

    while True:
        sl = len(seen)
        for i in seen.copy():
            if i.won:
                continue

            w = i.clone()
            if w.w() and w not in seen:
                seen.add(w)
           
            a = i.clone()
            if a.a() and a not in seen:
                seen.add(a)
        
            s = i.clone()
            if s.s() and s not in seen:
                seen.add(s)
    
            d = i.clone()
            if d.d() and d not in seen:
                seen.add(d)

        if len(seen) == sl:
            break

    for i in seen.copy():
        if i.won:
            if only_moves:
                print(i.history, end="")
                break
            else:
                print("======\nMaze " + str(maze_i))
                print(i.to_str())
                print("Found win: " + i.history)
                print("Visited: " + str(i.visited))
            visited.add(i.visited)

    if len(visited) > 1:
        raise Exception("Maze has multiple solutions, with distinct squares-visited patterns")

for i, maze in enumerate(mazes):
    solve_instance(maze, i+1)
