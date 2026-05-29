import json
import copy  # use it for deepcopy if needed
import math  # for math.inf
import logging

logging.basicConfig(format='%(levelname)s - %(asctime)s - %(message)s', datefmt='%d-%b-%y %H:%M:%S',
                    level=logging.INFO)

# Global variables in which you need to store player strategies (this is data structure that'll be used for evaluation)
# Mapping from histories (str) to probability distribution over actions
strategy_dict_x = {}
strategy_dict_o = {}


class History:
    def __init__(self, history=None):
        """
        # self.history : Eg: [0, 4, 2, 5]
            keeps track of sequence of actions played since the beginning of the game.
            Each action is an integer between 0-8 representing the square in which the move will be played as shown
            below.
              ___ ___ ____
             |_0_|_1_|_2_|
             |_3_|_4_|_5_|
             |_6_|_7_|_8_|

        # self.board
            empty squares are represented using '0' and occupied squares are either 'x' or 'o'.
            Eg: ['x', '0', 'x', '0', 'o', 'o', '0', '0', '0']
            for board
              ___ ___ ____
             |_x_|___|_x_|
             |___|_o_|_o_|
             |___|___|___|

        # self.player: 'x' or 'o'
            Player whose turn it is at the current history/board

        :param history: list keeps track of sequence of actions played since the beginning of the game.
        """
        if history is not None:
            self.history = history
            self.board = self.get_board()
        else:
            self.history = []
            self.board = ['0', '0', '0', '0', '0', '0', '0', '0', '0']
        self.player = self.current_player()

    def current_player(self):
        """ Player function
        Get player whose turn it is at the current history/board
        :return: 'x' or 'o' or None
        """
        total_num_moves = len(self.history)
        if total_num_moves < 9:
            if total_num_moves % 2 == 0:
                return 'x'
            else:
                return 'o'
        else:
            return None

    def get_board(self):
        """ Play out the current self.history and get the board corresponding to the history in self.board.

        :return: list Eg: ['x', '0', 'x', '0', 'o', 'o', '0', '0', '0']
        """
        board = ['0', '0', '0', '0', '0', '0', '0', '0', '0']
        for i in range(len(self.history)):
            if i % 2 == 0:
                board[self.history[i]] = 'x'
            else:
                board[self.history[i]] = 'o'
        return board

    def is_win(self):
        for a, b, c in WINNING_LINES:
            if self.board[a] != '0' and self.board[a] == self.board[b] == self.board[c]:
                return self.board[a]
        return False

    def is_draw(self):
         return (not self.is_win()) and all(sq != '0' for sq in self.board)
    def get_valid_actions(self):
        return [i for i, sq in enumerate(self.board) if sq == '0']

    def is_terminal_history(self):
        """Terminal if someone has won or the board is full."""
        return bool(self.is_win()) or all(sq != '0' for sq in self.board)

    def get_utility_given_terminal_history(self):
        """Utility from x's perspective: +1 if x wins, -1 if o wins, 0 for draw."""
        winner = self.is_win()
        if winner == 'x':
            return 1
        elif winner == 'o':
            return -1
        else:
            return 0

    def update_history(self, action):
        """Return a new History object with `action` appended (does not mutate self)."""
        new_history = self.history + [action]
        return History(new_history)


def backward_induction(history_obj):
    """
    :param history_obj: Histroy class object
    :return: best achievable utility (float) for th current history_obj
    """
    global strategy_dict_x, strategy_dict_o
    if history_obj.is_terminal_history():
        return history_obj.get_utility_given_terminal_history()
 
    history_key = ''.join(str(a) for a in history_obj.history)
    valid_actions = history_obj.get_valid_actions()
    player = history_obj.player
    best_action = valid_actions[0]
 
    if player == 'x':
        # Maximizer
        best_value = -math.inf
        for action in valid_actions:
            child = history_obj.update_history(action)
            value = backward_induction(child, alpha, beta)
            if value > best_value:
                best_value = value
                best_action = action
            alpha = max(alpha, best_value)
            if alpha >= beta:
                break  # beta cutoff: o would never let x reach this node
        strategy_dict_x[history_key] = {str(a): (1 if a == best_action else 0) for a in range(9)}
        return best_value
    else:
        # Minimizer (o)
        best_value = math.inf
        for action in valid_actions:
            child = history_obj.update_history(action)
            value = backward_induction(child, alpha, beta)
            if value < best_value:
                best_value = value
                best_action = action
            beta = min(beta, best_value)
            if alpha >= beta:
                break  # alpha cutoff: x would never let o reach this node
        strategy_dict_o[history_key] = {str(a): (1 if a == best_action else 0) for a in range(9)}
        return best_value


def solve_tictactoe():
    backward_induction(History())
    with open('./policy_x.json', 'w') as f:
        json.dump(strategy_dict_x, f)
    with open('./policy_o.json', 'w') as f:
        json.dump(strategy_dict_o, f)
    return strategy_dict_x, strategy_dict_o


if __name__ == "__main__":
    logging.info("Start")
    solve_tictactoe()
    logging.info("End")
