import sys


def solve(a):
    n = len(a)
    if n == 0:
        return 0
    # dp[i][j] = best achievable (current - opponent) on a[i..j]
    dp = [[0] * n for _ in range(n)]
    for i in range(n):
        dp[i][i] = a[i]
    for length in range(2, n + 1):
        for i in range(n - length + 1):
            j = i + length - 1
            dp[i][j] = max(a[i] - dp[i + 1][j], a[j] - dp[i][j - 1])
    return dp[0][n - 1]


def main():
    tokens = sys.stdin.read().split()
    if not tokens:
        return
    n = int(tokens[0])
    a = [int(x) for x in tokens[1:1 + n]]
    diff = solve(a)
    if diff > 0:
        print("Player 1 wins")
    elif diff < 0:
        print("Player 2 wins")
    else:
        print("Its a draw")


if __name__ == "__main__":
    main()# Write your code here
