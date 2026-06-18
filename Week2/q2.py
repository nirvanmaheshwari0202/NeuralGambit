import copy  # use it for deepcopy if needed
import math
import logging

logging.basicConfig(format='%(levelname)s - %(asctime)s - %(message)s', datefmt='%d-%b-%y %H:%M:%S',
                    level=logging.INFO)

board_positions_val_dict = {}
visited_histories_list = []

# The 8 symmetries (dihedral group D4) of a 3x3 board, written as index
# permutations: applying transform g maps the value at position i to g[i].
# Used to avoid re-exploring moves that are equivalent under board symmetry.
_D4 = (
    (0, 1, 2, 3, 4, 5, 6, 7, 8),   # identity
    (6, 3, 0, 7, 4, 1, 8, 5, 2),   # rotate 90
    (8, 7, 6, 5, 4, 3, 2, 1, 0),   # rotate 180
    (2, 5, 8, 1, 4, 7, 0, 3, 6),   # rotate 270
    (2, 1, 0, 5, 4, 3, 8, 7, 6),   # reflect left-right
    (6, 7, 8, 3, 4, 5, 0, 1, 2),   # reflect up-down
    (0, 3, 6, 1, 4, 7, 2, 5, 8),   # transpose
    (8, 5, 2, 7, 4, 1, 6, 3, 0),   # anti-transpose
)


class History:
    def __init__(self, num_boards=2, history=None):
        self.num_boards = num_boards
        if history is not None:
            self.history = history
            self.boards = self.get_boards()
        else:
            self.history = []
            self.boards = []
            for i in range(self.num_boards):
                self.boards.append(['0', '0', '0', '0', '0', '0', '0', '0', '0'])
        self.active_board_stats = self.check_active_boards()
        self.current_player = self.get_current_player()

    def get_boards(self):
        boards = []
        for i in range(self.num_boards):
            boards.append(['0', '0', '0', '0', '0', '0', '0', '0', '0'])
        for i in range(len(self.history)):
            board_num = math.floor(self.history[i] / 9)
            play_position = self.history[i] % 9
            boards[board_num][play_position] = 'x'
        return boards

    def check_active_boards(self):
        active_board_stat = []
        for i in range(self.num_boards):
            if self.is_board_win(self.boards[i]):
                active_board_stat.append(0)
            else:
                active_board_stat.append(1)
        return active_board_stat

    @staticmethod
    def is_board_win(board):
        for i in range(3):
            if board[3 * i] == board[3 * i + 1] == board[3 * i + 2] != '0':
                return True
            if board[i] == board[i + 3] == board[i + 6] != '0':
                return True
        if board[0] == board[4] == board[8] != '0':
            return True
        if board[2] == board[4] == board[6] != '0':
            return True
        return False

    def get_current_player(self):
        total_num_moves = len(self.history)
        if total_num_moves % 2 == 0:
            return 1
        else:
            return 2

    def get_boards_str(self):
        boards_str = ""
        for i in range(self.num_boards):
            boards_str = boards_str + ''.join([str(j) for j in self.boards[i]])
        return boards_str

    # ------------------------------------------------------------------ #
    #  Completed helper methods                                          #
    # ------------------------------------------------------------------ #
    def is_win(self):
        """The game is decided (terminal) iff every board is dead, i.e. no
        board is still active. Returns True in that case."""
        for stat in self.active_board_stats:
            if stat == 1:
                return False
        return True

    def get_valid_actions(self):
        """Legal moves: place an 'X' on an EMPTY square of an ACTIVE board.

        Two value-preserving symmetry reductions are applied so the search
        stays tractable (the 'specific move order' for more pruning / less
        memory):

          1. Identical boards are interchangeable, so among boards with the
             same configuration only the first is expanded.
          2. Within a board, empty squares that are equivalent under that
             board's own symmetry group (its stabiliser in D4) lead to
             identical sub-games, so only one representative is kept.

        Actions are returned in increasing index order, which also fills a
        board before moving to the next one -> good alpha-beta ordering.
        """
        valid_actions = []
        seen_boards = set()
        for board_num in range(self.num_boards):
            if self.active_board_stats[board_num] != 1:
                continue
            board = self.boards[board_num]
            cfg = ''.join(board)
            if cfg in seen_boards:          # reduction 1: duplicate board
                continue
            seen_boards.add(cfg)
            # stabiliser: the symmetries that leave THIS board unchanged
            stab = [g for g in _D4 if all(board[g[i]] == board[i] for i in range(9))]
            reps = set()
            for pos in range(9):
                if board[pos] != '0':
                    continue
                orbit_rep = min(g[pos] for g in stab)   # reduction 2
                if orbit_rep in reps:
                    continue
                reps.add(orbit_rep)
                valid_actions.append(9 * board_num + pos)
        return valid_actions

    def is_terminal_history(self):
        """Terminal when all boards are dead (no active board remains)."""
        return self.is_win()

    def get_value_given_terminal_history(self):
        """At a terminal history the player who JUST moved completed the last
        three-in-a-row and therefore loses; the player to move is the winner.
        Player 1 is the maximiser, so the value (utility for player 1) is +1
        if it is player 1's turn at the terminal node, else -1."""
        if self.current_player == 1:
            return 1
        else:
            return -1


def alpha_beta_pruning(history_obj, alpha, beta, max_player_flag):
    """Maxmin value of the game via alpha-beta pruning."""
    global visited_histories_list
    visited_histories_list.append(history_obj.history)

    if history_obj.is_terminal_history():
        return history_obj.get_value_given_terminal_history()

    if max_player_flag:
        value = -math.inf
        for action in history_obj.get_valid_actions():
            child = History(num_boards=history_obj.num_boards,
                            history=history_obj.history + [action])
            value = max(value, alpha_beta_pruning(child, alpha, beta, False))
            alpha = max(alpha, value)
            if alpha >= beta:               # beta cut-off
                break
        return value
    else:
        value = math.inf
        for action in history_obj.get_valid_actions():
            child = History(num_boards=history_obj.num_boards,
                            history=history_obj.history + [action])
            value = min(value, alpha_beta_pruning(child, alpha, beta, True))
            beta = min(beta, value)
            if alpha >= beta:               # alpha cut-off
                break
        return value


def maxmin(history_obj, max_player_flag):
    """Maxmin value with memoisation over board positions. Because the number
    of X's in a position fixes whose turn it is, the board string alone is a
    sound memo key."""
    global board_positions_val_dict

    if history_obj.is_terminal_history():
        return history_obj.get_value_given_terminal_history()

    key = history_obj.get_boards_str()
    if key in board_positions_val_dict:
        return board_positions_val_dict[key]

    if max_player_flag:
        value = -math.inf
        for action in history_obj.get_valid_actions():
            child = History(num_boards=history_obj.num_boards,
                            history=history_obj.history + [action])
            value = max(value, maxmin(child, False))
    else:
        value = math.inf
        for action in history_obj.get_valid_actions():
            child = History(num_boards=history_obj.num_boards,
                            history=history_obj.history + [action])
            value = min(value, maxmin(child, True))

    board_positions_val_dict[key] = value
    return value


def solve_alpha_beta_pruning(history_obj, alpha, beta, max_player_flag):
    global visited_histories_list
    val = alpha_beta_pruning(history_obj, alpha, beta, max_player_flag)
    return val, visited_histories_list


if __name__ == "__main__":
    logging.info("start")
    logging.info("alpha beta pruning")
    value, visited_histories = solve_alpha_beta_pruning(History(history=[], num_boards=2), -math.inf, math.inf, True)
    logging.info("maxmin value {}".format(value))
    logging.info("Number of histories visited {}".format(len(visited_histories)))
    logging.info("maxmin memory")
    logging.info("maxmin value {}".format(maxmin(History(history=[], num_boards=2), True)))
    logging.info("end")
