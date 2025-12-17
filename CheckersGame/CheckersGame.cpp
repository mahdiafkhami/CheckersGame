/*
  Checkers / Draughts (Console) - C++
  ----------------------------------
  Rules implemented (American checkers style):
  - Pieces move diagonally on dark squares only.
  - Normal man moves 1 step forward (white goes down, black goes up).
  - Capture is a 2-step diagonal jump over an enemy piece.
  - If any capture is available, capturing is mandatory.
  - Multi-capture (chain jumps) in the same turn is enforced.
  - Promotion: a man becomes a king when reaching the last row.
  - King moves/captures like a man BUT in all 4 diagonal directions (still 1-step / 2-step jumps).

  Input:
  - From-to format like:  b6 a5
  - During multi-capture, only enter next destination square like: c3
*/


#ifdef _WIN32
#include <Windows.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <cstdlib>   // for system()

using namespace std;

/*
  We represent pieces with integers:
  0 = empty
  1 = white man
  2 = white king
  3 = black man
  4 = black king
*/
enum Piece {
    EMPTY = 0,
    W_MAN = 1,
    W_KING = 2,
    B_MAN = 3,
    B_KING = 4
};

/*
  Player turn:
  WHITE = Player 1
  BLACK = Player 2
*/
enum Player {
    WHITE = 1,
    BLACK = 2
};

/*
  A Move structure describes one move:
  - from (fr, fc) to (tr, tc)
  - isCapture: whether it jumps over an enemy
  - (cr, cc): the captured enemy location (only valid if isCapture == true)
*/
struct Move {
    int fr, fc;
    int tr, tc;
    bool isCapture;
    int cr, cc;
};

// 8x8 board
static int board[8][8];

/* ------------------ Utility helpers ------------------ */

// Check if (r,c) is inside the 8x8 board
static bool inBounds(int r, int c) {
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

// In checkers, only dark squares are used.
// With this coordinate system (0-based), dark squares are where (r+c) is odd.
static bool isDarkSquare(int r, int c) {
    return (r + c) % 2 == 1;
}

// Is the piece a king?
static bool isKing(Piece p) {
    return p == W_KING || p == B_KING;
}

// Does piece p belong to player pl?
static bool belongsTo(Piece p, Player pl) {
    if (p == EMPTY) return false;
    if (pl == WHITE) return p == W_MAN || p == W_KING;
    return p == B_MAN || p == B_KING;
}

// Are two pieces enemies? (white vs black)
static bool isEnemy(Piece a, Piece b) {
    if (a == EMPTY || b == EMPTY) return false;
    bool aWhite = (a == W_MAN || a == W_KING);
    bool bWhite = (b == W_MAN || b == W_KING);
    return aWhite != bWhite;
}

// Printable symbols for each piece (feel free to change)
static string pieceStr(Piece p) {
    switch (p) {
    case EMPTY:  return "    ";
    case W_MAN:  return " WB ";
    case W_KING: return " WK ";
    case B_MAN:  return " BM ";
    case B_KING: return " BK ";
    default:     return " ? ";
    }
}

// Clear console screen (Windows vs Linux/macOS)
static void clearScreen() {
#if __linux__
    system("clear");
#else
    system("cls");
#endif
}

/* ------------------ Board setup & rendering ------------------ */

// Initialize board to standard checkers start position
static void initBoard() {
    // 1) clear everything
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            board[r][c] = EMPTY;

    // 2) place white men on rows 0..2 on dark squares
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 8; c++) {
            if (isDarkSquare(r, c)) board[r][c] = W_MAN;
        }
    }

    // 3) place black men on rows 5..7 on dark squares
    for (int r = 5; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (isDarkSquare(r, c)) board[r][c] = B_MAN;
        }
    }
}

// Print the board with coordinates like chess: columns a-h and rows 1-8
static void printBoard() {
    cout << "  +----+----+----+----+----+----+----+----+\n";
    for (int r = 0; r < 8; r++) {
        cout << (r + 1) << " |";
        for (int c = 0; c < 8; c++) {
            cout << pieceStr((Piece)board[r][c]) << "|";
        }
        cout << "\n  +----+----+----+----+----+----+----+----+\n";
    }
    cout << "    a    b    c    d    e    f    g    h\n";
}

// Convert (r,c) -> "b6" for user friendly printing
static string sqToStr(int r, int c) {
    string s;
    s += char('a' + c);
    s += char('1' + r);
    return s;
}

/*
  Parse a square from input token.
  Example tokens: "b6", "B6", "b6," etc.
  We search for:
  - first letter a-h => file (column)
  - first digit 1-8  => rank (row)
*/
static bool parseSquare(const string& token, int& r, int& c) {
    if (token.size() < 2) return false;

    char file = 0;
    char rank = 0;

    for (char ch : token) {
        if (isalpha((unsigned char)ch)) { file = (char)tolower(ch); break; }
    }
    for (char ch : token) {
        if (isdigit((unsigned char)ch)) { rank = ch; break; }
    }

    if (file < 'a' || file > 'h') return false;
    if (rank < '1' || rank > '8') return false;

    c = file - 'a';
    r = rank - '1';
    return inBounds(r, c);
}

/*
  Forward direction:
  - White moves downward => +1 row
  - Black moves upward   => -1 row
*/
static int forwardDir(Player pl) {
    return (pl == WHITE) ? +1 : -1;
}

/* ------------------ Move generation (core rules) ------------------ */

/*
  Generate all capture moves FROM a single piece at (r,c).
  For men:
    - can capture only forward diagonals (2 steps)
  For kings:
    - can capture in all 4 diagonals (2 steps)
*/
static vector<Move> captureMovesFrom(int r, int c, Player pl) {
    vector<Move> moves;

    Piece p = (Piece)board[r][c];
    if (!belongsTo(p, pl)) return moves;

    // direction deltas for diagonal movement
    vector<pair<int, int>> dirs;
    if (isKing(p)) {
        dirs = { {+1,+1},{+1,-1},{-1,+1},{-1,-1} };
    }
    else {
        int d = forwardDir(pl);
        dirs = { {d,+1},{d,-1} };
    }

    // A capture checks:
    //   adjacent square contains enemy
    //   landing square (2 steps away) is empty
    for (auto [dr, dc] : dirs) {
        int r1 = r + dr, c1 = c + dc;       // enemy position
        int r2 = r + 2 * dr, c2 = c + 2 * dc;     // landing position

        if (!inBounds(r1, c1) || !inBounds(r2, c2)) continue;
        if (board[r2][c2] != EMPTY) continue;

        if (isEnemy(p, (Piece)board[r1][c1])) {
            moves.push_back({ r,c,r2,c2,true,r1,c1 });
        }
    }

    return moves;
}

/*
  Generate all simple (non-capture) moves FROM a single piece at (r,c).
  For men:
    - 1 step forward diagonals
  For kings:
    - 1 step in any diagonal direction
*/
static vector<Move> simpleMovesFrom(int r, int c, Player pl) {
    vector<Move> moves;

    Piece p = (Piece)board[r][c];
    if (!belongsTo(p, pl)) return moves;

    vector<pair<int, int>> dirs;
    if (isKing(p)) {
        dirs = { {+1,+1},{+1,-1},{-1,+1},{-1,-1} };
    }
    else {
        int d = forwardDir(pl);
        dirs = { {d,+1},{d,-1} };
    }

    for (auto [dr, dc] : dirs) {
        int r2 = r + dr, c2 = c + dc;
        if (!inBounds(r2, c2)) continue;
        if (board[r2][c2] != EMPTY) continue;
        moves.push_back({ r,c,r2,c2,false,-1,-1 });
    }

    return moves;
}

/*
  Collect ALL capture moves available for a player on the whole board.
  This is important because capturing is mandatory:
  if any capture exists, player must choose a capture move.
*/
static vector<Move> allCaptures(Player pl) {
    vector<Move> res;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            auto m = captureMovesFrom(r, c, pl);
            res.insert(res.end(), m.begin(), m.end());
        }
    }
    return res;
}

/*
  Collect ALL legal moves for the player:
  - If captures exist => only capture moves are legal (mandatory capture rule)
  - Otherwise => all simple moves are legal
*/
static vector<Move> allLegalMoves(Player pl) {
    auto caps = allCaptures(pl);
    if (!caps.empty()) return caps;

    vector<Move> res;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            auto m = simpleMovesFrom(r, c, pl);
            res.insert(res.end(), m.begin(), m.end());
        }
    }
    return res;
}

// Count how many pieces a player has (used for win check)
static int countPieces(Player pl) {
    int cnt = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (belongsTo((Piece)board[r][c], pl))
                cnt++;
    return cnt;
}

// Compare a generated Move with user input (from->to)
static bool sameMove(const Move& a, int fr, int fc, int tr, int tc) {
    return a.fr == fr && a.fc == fc && a.tr == tr && a.tc == tc;
}

/*
  Apply a move to the board:
  - Move piece from (fr,fc) to (tr,tc)
  - Clear old position
  - If capture => remove captured enemy at (cr,cc)
*/
static void applyMove(const Move& mv) {
    Piece p = (Piece)board[mv.fr][mv.fc];
    board[mv.tr][mv.tc] = p;
    board[mv.fr][mv.fc] = EMPTY;

    if (mv.isCapture) {
        board[mv.cr][mv.cc] = EMPTY;
    }
}

/*
  Promotion rule:
  - White man becomes king when it reaches row 7
  - Black man becomes king when it reaches row 0
  Note: we promote at end of turn (after chain captures).
*/
static void maybePromote(int r, int c) {
    Piece p = (Piece)board[r][c];
    if (p == W_MAN && r == 7) board[r][c] = W_KING;
    if (p == B_MAN && r == 0) board[r][c] = B_KING;
}

/* ------------------ Main game loop ------------------ */

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    initBoard();

    Player turn = WHITE;

    while (true) {
        // Win condition 1: player has no pieces left
        if (countPieces(WHITE) == 0) {
            cout << "GAME OVER! BLACK wins (WHITE has no pieces).\n";
            break;
        }
        if (countPieces(BLACK) == 0) {
            cout << "GAME OVER! WHITE wins (BLACK has no pieces).\n";
            break;
        }

        // Determine legal moves for current player
        auto legal = allLegalMoves(turn);

        // Win condition 2: player has no legal moves => loses
        if (legal.empty()) {
            cout << "GAME OVER! " << (turn == WHITE ? "BLACK" : "WHITE")
                << " wins (opponent has no legal moves).\n";
            break;
        }

        clearScreen();
        printBoard();

        cout << "\nTurn: " << (turn == WHITE ? "WHITE (Player 1)" : "BLACK (Player 2)") << "\n";
        if (!allCaptures(turn).empty())
            cout << "Rule: Capture is available => you MUST capture.\n";

        cout << "Enter move like: b6 a5 (from to)\n> ";

        // Read two tokens from user (from-square and to-square)
        string t1, t2;
        if (!(cin >> t1 >> t2)) break;

        int fr, fc, tr, tc;
        if (!parseSquare(t1, fr, fc) || !parseSquare(t2, tr, tc)) {
            cout << "Invalid input format. Use like b6 a5\n";
            cin.clear();
            continue;
        }

        // Destination must be dark square
        if (!isDarkSquare(tr, tc)) {
            cout << "You can only move to dark squares.\n";
            continue;
        }

        // Must move your own piece
        Piece p = (Piece)board[fr][fc];
        if (!belongsTo(p, turn)) {
            cout << "That piece is not yours.\n";
            continue;
        }

        // Destination must be empty
        if (board[tr][tc] != EMPTY) {
            cout << "Destination is not empty.\n";
            continue;
        }

        // Find if user move matches one of currently legal moves
        auto it = find_if(legal.begin(), legal.end(), [&](const Move& m) {
            return sameMove(m, fr, fc, tr, tc);
            });

        if (it == legal.end()) {
            cout << "Illegal move.\n";
            continue;
        }

        // Apply the selected move
        Move mv = *it;
        applyMove(mv);

        // Track current piece position after the move (for multi-capture)
        int curR = mv.tr, curC = mv.tc;

        /*
          Multi-capture rule:
          If the move was a capture, and from the new position another capture is possible,
          the player must continue capturing with the SAME piece.
        */
        if (mv.isCapture) {
            while (true) {
                auto nextCaps = captureMovesFrom(curR, curC, turn);
                if (nextCaps.empty()) break;

                clearScreen();
                printBoard();

                cout << "\nMulti-capture required from " << sqToStr(curR, curC) << "\n";
                cout << "Possible next landings: ";
                for (auto& nm : nextCaps) cout << sqToStr(nm.tr, nm.tc) << " ";
                cout << "\nEnter next destination (e.g. c3):\n> ";

                string tnext;
                cin >> tnext;

                int nr, nc;
                if (!parseSquare(tnext, nr, nc)) {
                    cout << "Bad square input.\n";
                    continue;
                }

                // Must choose one of the forced capture landing squares
                auto it2 = find_if(nextCaps.begin(), nextCaps.end(), [&](const Move& m) {
                    return m.tr == nr && m.tc == nc;
                    });

                if (it2 == nextCaps.end()) {
                    cout << "You must continue capturing (choose one of the shown squares).\n";
                    continue;
                }

                applyMove(*it2);
                curR = it2->tr;
                curC = it2->tc;
            }
        }

        // Promotion happens at the end of the entire turn (after chain jumps)
        maybePromote(curR, curC);

        // Switch turns
        turn = (turn == WHITE) ? BLACK : WHITE;
    }

    return 0;
}
