#include <iostream>
#include <algorithm>
#include <cassert>
#include <random>
#include <tuple>
#include <stack>
#include <cstdlib>
#include <chrono>
using namespace std;

// デバッグ用
bool debug = false;
bool debug_time = false;

// 大域変数・定数
const int T = 9; // 全ターン数
const int P = 4; // プレイヤー(大名)数
const int N = 6; // 領主の数
const int remain_day[10] = {15, 15, 10, 10, 5, 5, 10, 10, 5, 5}; // 昼残り
const int remain_night[10] = {4, 4, 4, 2, 2, 0, 4, 2, 2, 0}; // 夜残り
int remain_votes[10]; // remain_day + remain_night * 2
int expected_votes[10]; // 昼なら5、夜なら9である。
const int noonpeople = 5;
const int nightpeople = 2;
int A[N]; // 領主の兵力
int total_score; // 兵力の合計値
int priority[N];
int x_now=0, y_now=0; // 現在の戦略
int top_lord=0, bottom_lord=0; // 1位を取る領主、3位を取る領主
int now_amari = 0;
bool bug[P]; // 相手が child process ended なっているかどうかを判定する。

// ターンでの変数
int turn; // 現在のターン
bool isnoon; // 昼か？
int B[T+1][N][P]; // B[turn][n][p] = プレイヤーpの領主nへの親密度
int R[T+1][N]; // 自分の本当の投票数
int W[N]; // 前の休日までの、明らかになっていない夜の交渉回数(自分を除く)
int W_turn[T+1][N]; // 控え
int people; // 出力数
int L[noonpeople]; // 出力

// ランダムで方針を決定するためのもの
const int RD_turn[10] = {0, 12000, 1500, 12000, 1500, 4000,
                         1500, 12000, 1500, 4000}; // RD = RD_turn[turn];
const int RD_M = 40000;
int RD;
int random_votes[T+1][RD_M][N][P];
int last_votes[T+1][RD_M][N][P]; // turn == 5 || turn == 9 のみ使用
int player_votes[P][RD_M][N][P]; // random_votes + last_votes を基本的に入れる
int rv_n_first[T+1][RD_M][N];
int rv_n_third[T+1][RD_M][N];
int rv_n_first_max[T+1][N];
int rv_n_third_max[T+1][N];
int rv_n_first_min[T+1][N];
int rv_n_third_min[T+1][N];
const double minimum_rate = 0.1; // 合格最低ライン
double minimum_score; // 合格最低点

// 深さ優先探査で総当りするためのもの
typedef tuple<int, int*> future;
stack<future> St;
int L_prep[9];
int conbi[10][7][6000][9]; // conbi[turn][depth][num][id]
int conbi_total[10][7]; // idの数

// 点数計算用
double points_zenhan[P];
const double epsilon = 0.00001;

// randomはメルセンヌ・ツイスターで作る
random_device rd;
mt19937 mt(rd());
int RS[N][P];
int RS_sum[P];

// 過去の傾向から乱数を作る
int randmod(int m) {
  return mt()%m;
}

void make_random_sheet() {
  int done_night = remain_night[0] - remain_night[turn];
  int bug_people = 0;
  for (int j=1; j<P; j++) {
    if (bug[j]) bug_people++;
  }
  int bug_zero = bug_people * done_night;
  for (int i=0; i<N; i++) {
    RS[i][0] = R[turn][i];
    for (int j=1; j<P; j++) {
      if (!bug[j]) {
        RS[i][j] = B[turn][i][j] + W[i];
        if (i == 0) {
          RS[i][j] -= bug_zero;
        }
        if (turn == 1 || turn == 2) {
          RS[i][j] += A[i]/turn;
        }
      }
    }
  }
  for (int j=1; j<P; j++) {
    if (bug[j]) {
      RS[0][j] = 1;
      for (int i=1; i<N; i++) {
        RS[i][j] = 0;
      }
    }
  }
  for (int j=0; j<P; j++) {
    for (int i=1; i<N; i++) {
      RS[i][j] += RS[i-1][j];
    }
    RS_sum[j] = RS[N-1][j];
  }
  if (debug) {
    for (int j=0; j<P; j++) {
      cerr << "Player " << j << ": ";
      for (int i=0; i<N; i++) {
        cerr << RS[i][j] << " ";
      }
      cerr << endl;
    }
  }
}

int randplay(int player) {
  int x = randmod(RS_sum[player]);
  for (int i=0; i<N; i++) {
    if (RS[i][player] > x) return i;
  }
  return 0;
}

void depth_init_dn(int depth, int num) { // 深さ優先探査して出来た組み合わせをconbiに入れる
  conbi_total[depth][num] = 0;
  int* Xp = (int*) malloc(sizeof (int) * 9);
  future F = make_tuple(0, Xp);
  St.push(F);
  while(!St.empty()) {
    int d = get<0>(St.top());
    int* p = get<1>(St.top());
    St.pop();
    if (d == 0) {
      for (int i=0; i<num; i++) {
        int* q = (int*) malloc(sizeof (int) * 9);
        q[d] = i;
        St.push(make_tuple(d+1, q));
      }
    } else if (d < depth) {
      for (int i=p[d-1]; i<num; i++) {
        int* q = (int*) malloc(sizeof (int) * 9);
        for (int j=0; j<d; j++) {
          q[j] = p[j];
        }
        q[d] = i;
        St.push(make_tuple(d+1, q));  
      }
    } else {
      for (int i=0; i<depth; i++) {
        conbi[depth][num][conbi_total[depth][num]][i] = p[i];
      }
      conbi_total[depth][num]++;
    }
    free(p);
  }
}

void depth_init() {
  for (int i=0; i<=9; i++) {
    for (int j=0; j<=6; j++) {
      depth_init_dn(i, j);
    }
  }
}

// 点数計算
void getpoint(int expect[N][P], double points[P]) {
  for (int i=0; i<N; i++) {
    int max_v = -1;
    int min_v = 10000;
    for (int j=0; j<P; j++) {
      if (max_v < expect[i][j]) max_v = expect[i][j];
      if (min_v > expect[i][j]) min_v = expect[i][j];
    }
    int max_p = 0;
    int min_p = 0;
    for (int j=0; j<P; j++) {
      if (max_v == expect[i][j]) max_p++;
      if (min_v == expect[i][j]) min_p++;
    }
    double max_gp = ((double)A[i])/max_p;
    double min_gp = ((double)A[i])/min_p;
    for (int j=0; j<P; j++) {
      if (max_v == expect[i][j]) points[j] += max_gp;
      if (min_v == expect[i][j]) points[j] -= min_gp;
    }    
  }
}

bool isnowtop(int expect[N][P]) {
  // トップかどうかを判定する。余裕のない勝ち方は勝ちと判定しない。
  double t_points[P];
  if (turn <= 5) {
    fill(t_points, t_points+P, 0);
  } else {
    for (int i=0; i<P; i++) {
      t_points[i] = points_zenhan[i];
    }
  }
  getpoint(expect, t_points);
  for (int i=1; i<P; i++) {
    if (t_points[0] + epsilon < t_points[i]) return false;
  }
  return (t_points[0] > minimum_score + points_zenhan[0]);
}

bool isnowtop_other(int expect[N][P], int p) {
  // Player p が トップかどうかを判定する。
  double t_points[P];
  if (turn <= 5) {
    fill(t_points, t_points+P, 0);
  } else {
    for (int i=0; i<P; i++) {
      t_points[i] = points_zenhan[i];
    }
  }
  getpoint(expect, t_points);
  for (int i=0; i<P; i++) {
    if (i == p) continue;
    if (t_points[p] + epsilon < t_points[i]) return false;
  }
  return true;
}


void determine_points_zenhan() { // 前半のポイントを計算する。
  fill(points_zenhan, points_zenhan+P, 0);
  getpoint(B[turn], points_zenhan);
  if (debug) {
    cerr << "points ";
    for (int j=0; j<P; j++) {
      cerr << points_zenhan[j] << " ";
    }
    cerr << endl;
  }
  // minimum_scoreの更新
  double temp_score[P];
  for (int j=0; j<P; j++) {
    temp_score[j] = points_zenhan[j];
  }
  sort(temp_score, temp_score+P);
  reverse(temp_score, temp_score+P);
  if (temp_score[0] - epsilon < points_zenhan[0]) { // 前半1位
    minimum_score = temp_score[1];
  } else if (temp_score[1] - epsilon < points_zenhan[0]) { // 前半2位
    minimum_score = temp_score[0];
  } else { // 前半3・4位 (実は2位も同じ計算式)
    minimum_score = temp_score[0] + temp_score[1] - points_zenhan[0];
  }
  minimum_score = max(total_score * minimum_rate, minimum_score + epsilon);
}

// child process ended 判定

void ended() {
  if (turn == 1 || turn == 6) return; // 1,6ターン目は判定しない
  for (int p=1; p<P; p++) {
    if (bug[p]) {
      if ((!isnoon) && (B[turn][0][p] - B[turn-1][0][p] < 5)) {
        bug[p] = false;
      } else if (isnoon && (W_turn[turn][0] - W_turn[turn-1][0] < 2)) {
        bug[p] = false;
      }
    } else if (turn == 2) { // bugか判定。bugが起こるなら1ターン目か2ターン目だろう。
      if (B[turn][0][p] - B[turn-1][0][p] >= 5) {
        bug[p] = true;
      }
    } else if (turn == 4) {
      if (B[turn][0][p] - B[turn-1][0][p] >= 5 
          && W_turn[turn-1][0] - W_turn[turn-2][0] >= 2) {
        bug[p] = true;
      }
    }
  }
  if (debug) {
    for (int p=1; p<P; p++) {
      if (bug[p]) cerr << "Player " << p << " is ended." << endl;
    }
  }
}

// 初期化・入力関連
void prep_init() {
  total_score = 0;
  fill(W, W+N, 0);
  // remain_votesを埋める
  for (int i=0; i<=T; i++) {
    remain_votes[i] = remain_day[i] + remain_night[i] * 2;
  }
  // expected_votesを埋める
  for (int i=0; i<=T; i++) {
    expected_votes[i] = ((i%2 == 1) ? 5 : 9);
  }
  // conbiを埋める。
  depth_init();
  // points_zenhan[0]だけは初期化する。
  points_zenhan[0] = 0;
  // 全員bugではない。
  fill(bug, bug+P, false);
  // 今のうちに last_votes は初期化しておく
  for (int x=0; x<=T; x++) {
    if (x == 5 || x == 9) {
      for (int t=0; t<RD_M; t++) {
        for (int i=0; i<N; i++) {
          for (int j=0; j<P; j++) {
            last_votes[x][t][i][j] = 0;
          }
        }
      }
    }
  }
}

void first_init() {
  int x;
  cin >> x;
  cin >> x;
  cin >> x;
  for (int i=0; i<N; i++) {
    cin >> A[i];
    total_score += A[i];
  }
  // 1ターン目投票用priorityの確定
  int min_score = 1000;
  for (int i=0; i<N; i++) {
    if (min_score > A[i]) min_score = A[i];
  }
  for (int i=N-1; i>=0; i--) {
    if (min_score == A[i]) {
      priority[N-1] = i;
    }
  }
  int j = 0;
  for (int i=0; i<N; i++) {
    if (i != priority[N-1]) {
      priority[j++] = i;
    }
  }
  // minimum_scoreの計算
  minimum_score = minimum_rate * total_score;
}

void turn_init() {
  cin >> turn;
  char D;
  cin >> D;
  isnoon = (D == 'D');
  if (isnoon) {
    people = noonpeople;
  } else {
    people = nightpeople;
  }
  for (int i=0; i<N; i++) {
    for (int j=0; j<P; j++) {
      cin >> B[turn][i][j];
    }
  }
  for (int i=0; i<N; i++) {
    cin >> R[turn][i];
  }
  if (turn == 6) {
    fill(W, W+P, 0);
    determine_points_zenhan();
  }
  if (isnoon) {
    for (int i=0; i<N; i++) {
      int w;
      cin >> w;
      W[i] += w - (R[turn][i] - R[turn-1][i])%2;
    }
  }
  for (int i=0; i<N; i++) {
    W_turn[turn][i] = W[i];
  }
  ended();
  RD = RD_turn[turn];
  make_random_sheet();
}

// 相手の投票数をランダムで求める

void random_write(int t) {
  // 明らかになっているところ
  for (int i=0; i<N; i++) {
    random_votes[turn][t][i][0] = R[turn][i];
    for (int j=1; j<P; j++) {
      random_votes[turn][t][i][j] = B[turn][i][j];
    }
  }  
  // 夜の交渉回数を過去を参考にしたランダムで割り振る
  int WR[N];
  for (int i=0; i<N; i++) {
    WR[i] = W[i];
  }
  int done_night = remain_night[0] - remain_night[turn];
  // bugになった人は2ターン目には死んでいる
  for (int j=1; j<P; j++) {
    if (bug[j]) {
      WR[0] -= done_night;
      random_votes[turn][t][0][j] += 2 * done_night;
    }
  }
  for (int j=1; j<P; j++) {
    if (!bug[j]) {
      for (int k=0; k<done_night; k++) {
        while (true) {
          int i = randplay(j);
          if (WR[i] > 0) {
            random_votes[turn][t][i][j] += 2;
            WR[i]--;
            break;
          }
        }
      }
    }
  }
  // 未来の交渉回数を過去を参考にしたランダムで割り振る
  if (turn == 5 || turn == 9) { // turn == 5 || turn == 9 は別枠に保存
    for (int j=0; j<P; j++) {
      for (int k=0; k<remain_votes[turn]; k++) {
        int i = randplay(j);
        last_votes[turn][t][i][j]++;
      }
    }
  } else {
    for (int j=1; j<P; j++) {
      for (int k=0; k<remain_votes[turn]; k++) {
        int i = randplay(j);
        random_votes[turn][t][i][j]++;
      }
    }
  }
}

void random_first_third(int t) {
  int temp[P-1];
  for (int i=0; i<N; i++) {
    for (int j=1; j<P; j++) {
      temp[j-1] = random_votes[turn][t][i][j];
    }
    sort(temp, temp+P-1);
    rv_n_first[turn][t][i] = max(temp[P-2] - R[turn][i] + 1, 0);
    rv_n_third[turn][t][i] = max(temp[0] - R[turn][i] + 1, 0);
  }
}

void random_sev() {
  for (int t=0; t<RD; t++) {
    random_write(t);
    random_first_third(t);
  }
  for (int i=0; i<N; i++) {
    rv_n_first_max[turn][i] = -1000;
    rv_n_first_min[turn][i] = 1000;
    rv_n_third_max[turn][i] = -1000;
    rv_n_third_min[turn][i] = 1000;
    for (int t=0; t<RD; t++) {
      if (rv_n_first_max[turn][i] < rv_n_first[turn][t][i]) {
        rv_n_first_max[turn][i] = rv_n_first[turn][t][i];
      }
      if (rv_n_first_min[turn][i] > rv_n_first[turn][t][i]) {
        rv_n_first_min[turn][i] = rv_n_first[turn][t][i];
      }
      if (rv_n_third_max[turn][i] < rv_n_third[turn][t][i]) {
        rv_n_third_max[turn][i] = rv_n_third[turn][t][i];
      }
      if (rv_n_third_min[turn][i] > rv_n_third[turn][t][i]) {
        rv_n_third_min[turn][i] = rv_n_third[turn][t][i];
      }
    }
  }
}
 
// 1、2、3、6、7ターン目は、戦略を決定した後、全探査する。

// 戦略の決定

bool isvalid(int x, int y) { // xはtopに1が立っている。yはtopとbottomに1が立っている。xに1が立っているのにyに0が立っている場合はfalseを返す。そうでなければ有効なのでtrueを返す。
  for (int i=0; i<N; i++) {
    if ( (((x >> i) & 1) == 1) && (((y >> i) & 1) == 0) ) {
      return false;
    }
  }
  return true;
}

int score(int x, int y) {
  int ret = 0;
  for (int i=0; i<N; i++) {
    if ((x >> i) & 1) {
      ret += A[i];
    }
    if (((y >> i) & 1) == 0) {
      ret -= A[i];
    }
  }
  return ret;
}

void determine_priority() {
  int max_x = x_now;
  int max_y = y_now;
  random_sev();
  int max_win = -100;
  for (int x=0; x< (1 << N); x++) {
    for (int y=0; y< (1 << N); y++) {
      if (isvalid(x, y) && score(x, y) > -1 * points_zenhan[0]) {
        int win = 0;
        for (int t=0; t<RD; t++) {
          // 残りターン数で実現可能か
          int need_votes = 0;
          for (int i=0; i<N; i++) {
            if ( ((x >> i) & 1) == 1 ) {
              need_votes += rv_n_first[turn][t][i];
            } else if ( ((y >> i) & 1) == 1 ) {
              need_votes += rv_n_third[turn][t][i];
            }
          }
          if (need_votes > remain_votes[turn]) continue;
          // これで勝てているか
          for (int i=0; i<N; i++) {          
            if ( ((x >> i) & 1) == 1 ) {
              random_votes[turn][t][i][0] = R[turn][i] + rv_n_first[turn][t][i];
            } else if ( ((y >> i) & 1) == 1 ) {
              random_votes[turn][t][i][0] = R[turn][i] + rv_n_third[turn][t][i];
            } else {
              random_votes[turn][t][i][0] = R[turn][i];
            }
          }
          if (isnowtop(random_votes[turn][t])) {
            win++;
          }          
        }
        // 「勝った回数」が最高なら記録
        if (max_win < win) {
          max_win = win;
          max_x = x;
          max_y = y;
        }
      }
    }
  }
  // 採用された戦略を記録する。
  x_now = max_x;
  y_now = max_y;
  pair<int, int> temp_priority[N];
  top_lord = 0;
  bottom_lord = 0;
  for (int i=0; i<N; i++) {
    if ( (x_now >> i) & 1 ) {
      temp_priority[i] = make_pair(0, i);
      top_lord++;
    } else if ( (y_now >> i) & 1 ) {
      temp_priority[i] = make_pair(1, i);
      bottom_lord++;
    } else {
      temp_priority[i] = make_pair(2, i);
    }
  }
  sort(temp_priority, temp_priority+N);
  if (debug) {
    cerr << "max_win: " << max_win << " of " << RD_turn[turn] << endl;
    cerr << "priority: ";
  }
  for (int i=0; i<N; i++) {
    priority[i] = temp_priority[i].second;
    if (debug) cerr << priority[i] << " ";
  }
  if (debug) {
    cerr << endl;
    cerr << "top_lord: " << top_lord << endl;
    cerr << "bottom_lord: " << bottom_lord << endl;
  }
  // ここから先は、L_prepを求める。
  int tlord = top_lord+bottom_lord;
  int conbi_t = conbi_total[expected_votes[turn]][tlord];
  max_win = -100;
  int max_c = 0;
  for (int c=0; c<conbi_t; c++) {
    int win = 0;
    int tvotes[N];
    fill(tvotes, tvotes+N, 0);
    for (int i=0; i<expected_votes[turn]; i++) {
      tvotes[priority[conbi[expected_votes[turn]][tlord][c][i]]]++;
    }
    for (int t=0; t<RD; t++) {
      // 残りターン数で実現可能か
      int need_votes = 0;
      int need_v[N];
      fill(need_v, need_v+N, 0);
      for (int i=0; i<N; i++) {
        if ( ((x_now >> i) & 1) == 1 ) {
          need_v[i] = max(rv_n_first[turn][t][i] - tvotes[i], 0) + tvotes[i];
          need_votes += need_v[i];
        } else if ( ((y_now >> i) & 1) == 1 ) {
          need_v[i] = max(rv_n_third[turn][t][i] - tvotes[i], 0) + tvotes[i];
          need_votes += need_v[i];
        }
      }
      if (need_votes > remain_votes[turn]) continue;
      // これで勝てているか
      for (int i=0; i<N; i++) {          
        if ( ((x_now >> i) & 1) == 1 ) {
          random_votes[turn][t][i][0] = R[turn][i] + need_v[i];
        } else if ( ((y_now >> i) & 1) == 1 ) {
          random_votes[turn][t][i][0] = R[turn][i] + need_v[i];
        } else {
          random_votes[turn][t][i][0] = R[turn][i];
        }
      }
      if (isnowtop(random_votes[turn][t])) {
        win++;
      }
    }
    // 「勝った回数」が最高なら記録
    if (max_win < win) {
      max_win = win;
      max_c = c;
    }
  }
  for (int i=0; i<expected_votes[turn]; i++) {
    L_prep[i] = priority[conbi[expected_votes[turn]][tlord][max_c][i]];
  }
  if (debug) {
    cerr << "max_win: " << max_win << " of " << RD_turn[turn] << endl;
    cerr << "L_prep: ";
    for (int i=0; i<expected_votes[turn]; i++) {
      cerr << L_prep[i] << " ";
    }
    cerr << endl;
    cerr << "Used conbi: ";
    for (int i=0; i<expected_votes[turn]; i++) {
      cerr << conbi[expected_votes[turn]][tlord][max_c][i] << " ";
    }
    cerr << endl;
  }
}

// 4, 5, 8, 9 ターン目は、投票の総当りで動く。

int count_win(int* q) {
  int ans = 0;
  for (int t=0; t<RD; t++) {
    for (int i=0; i<N; i++) {
      random_votes[turn][t][i][0] = R[turn][i];
    }
    for (int l=0; l<expected_votes[turn]; l++) {
      int i = priority[q[l]];
      random_votes[turn][t][i][0]++;
    }
    if (isnowtop(random_votes[turn][t])) {
      ans++;
    }
  }
  return ans;
}

void depth(int num) {
  int max_win = -100;
  int max_id = 0;
  int tot = conbi_total[expected_votes[turn]][num];
  for (int i=0; i<tot; i++) {
    int win = count_win(conbi[expected_votes[turn]][num][i]);
    if (max_win < win) {
      max_win = win;
      max_id = i;
    }
  }
  for (int i=0; i<expected_votes[turn]; i++) {
    L_prep[i] = priority[conbi[expected_votes[turn]][num][max_id][i]];
  }
  if (debug) {
    cerr << "max_win: " << max_win << " of " << RD_turn[turn] << endl;
    cerr << "L_prep: ";
    for (int i=0; i<expected_votes[turn]; i++) {
      cerr << L_prep[i] << " ";
    }
    cerr << endl;
    cerr << "Used conbi: ";
    for (int i=0; i<expected_votes[turn]; i++) {
      cerr << conbi[expected_votes[turn]][num][max_id][i] << " ";
    }
    cerr << endl;
  }
}

void depth_last() {
  int max_id[P]; // 各プレイヤーの最適戦略
  int num = N;
  int tot = conbi_total[expected_votes[turn]][num];
  int max_win[P];
  fill(max_win, max_win+P, -100);
  for (int p=1; p<P; p++) {
    if (bug[p]) {
      max_id[p] = tot-1;
      continue;
    }
    for (int t=0; t<RD; t++) {
      for (int i=0; i<N; i++) {
        for (int j=0; j<P; j++) {
          if (p != j) {
            player_votes[p][t][i][j] 
              = last_votes[turn][t][i][j] + random_votes[turn][t][i][j];
          }
        }
      }
    }
    for (int x=0; x<tot; x++) {
      int win = 0;
      for (int t=0; t<RD; t++) {
        for (int i=0; i<N; i++) {
          player_votes[p][t][i][p] = random_votes[turn][t][i][p];
        }
        for (int i=0; i<expected_votes[turn]; i++) {
          player_votes[p][t][conbi[expected_votes[turn]][num][x][i]][p]++;
        }
        if (isnowtop_other(player_votes[p][t], p)) {
          win++;
        }
      }
      if (max_win[p] < win) {
        max_win[p] = win;
        max_id[p] = x;
      }
    }
  }
  if (debug) {
    cerr << "The best strategy" << endl;
    for (int p=1; p<P; p++) {
      cerr << "Player " << p << " :";
      for (int i=0; i<expected_votes[turn]; i++) {
        cerr << conbi[expected_votes[turn]][num][max_id[p]][i] << " ";
      }
      cerr << "wins " << max_win[p] << endl;
    }
  }
  for (int t=0; t<RD; t++) {
    for (int i=0; i<N; i++) {
      for (int j=1; j<P; j++) {
        player_votes[0][t][i][j] = random_votes[turn][t][i][j];
      }
    }
    for (int j=1; j<P; j++) {
      for (int i=0; i<expected_votes[turn]; i++) {
        player_votes[0][t][conbi[expected_votes[turn]][num][max_id[j]][i]][j]++;
      }
    }
  }
  for (int x=0; x<tot; x++) {
    int win_p = 0;
    for (int t=0; t<RD; t++) {
      for (int i=0; i<N; i++) {
        player_votes[0][t][i][0] = R[turn][i];
      }
      for (int i=0; i<expected_votes[turn]; i++) {
        player_votes[0][t][conbi[expected_votes[turn]][num][x][i]][0]++;
      }
      if (isnowtop(player_votes[0][t])) {
        win_p++;
      }
    }
    int win = win_p;
    if (max_win[0] < win) {
      max_win[0] = win;
      max_id[0] = x;
    }
  }
  for (int i=0; i<expected_votes[turn]; i++) {
    L_prep[i] = conbi[expected_votes[turn]][num][max_id[0]][i];
  }
  if (debug) {
    cerr << "max_win: " << max_win[0] << " of " << RD_turn[turn] << endl;
    cerr << "L_prep: ";
    for (int i=0; i<expected_votes[turn]; i++) {
      cerr << L_prep[i] << " ";
    }
    cerr << endl;
  }
}

// 実行
void determine_L() {
  if (turn == 1) {
    for (int i=0; i<people; i++) {
      L[i] = priority[i];
    }
    return;
  }
  if (turn == 1 || turn == 2 || turn == 3 || turn == 6 || turn == 7) {
    determine_priority();
  } else { // turn == 4 || turn == 5 || turn == 8 || turn == 9
    for (int t=0; t<RD; t++) {
      random_write(t);
    }
    if (turn == 4 || turn == 8) {
      depth(N);
    } else {
      depth_last();
    }
  }
  if (!isnoon) {
    int temp[N];
    fill(temp, temp+N, 0);
    for (int i=0; i<expected_votes[turn]; i++) {
      temp[L_prep[i]]++; 
    }
    int min_of_max = 100;
    int saiyo = 0;
    bool saiyo_zorome = true;
    for (int c=0; c<conbi_total[people][N]; c++) {
      int temp_c[N];
      for (int i=0; i<N; i++) {
        temp_c[i] = temp[i];
      }      
      for (int i=0; i<people; i++) {
        temp_c[conbi[people][N][c][i]] -= 2;
      }
      bool votable = true;
      for (int i=0; i<N; i++) {
        if (temp_c[i] < 0) votable = false;
      }
      if (!votable) continue;
      int max = -100;
      for (int i=0; i<N; i++) {
        if (max < temp_c[i]) max = temp_c[i];
      }
      if (min_of_max > max) {
        saiyo_zorome = (conbi[people][N][c][0] == conbi[people][N][c][1]);
        min_of_max = max;
        saiyo = c;
      } else if (min_of_max == max) {
        if (saiyo_zorome 
            && (conbi[people][N][c][0] != conbi[people][N][c][1])) {
          saiyo_zorome = false;
          saiyo = c;
        }
      }
    }
    for (int i=0; i<people; i++) {
      L[i] = conbi[people][N][saiyo][i];
    }
  } else {
    for (int i=0; i<people; i++) {
      L[i] = L_prep[i];
    }
  }
}

// 出力
void turn_output() {
  determine_L();
  sort(L, L+people);
  for (int i=0; i<people; i++) {
    cout << L[i];
    if (i != people-1) {
      cout << " ";
    }
  }
  cout << endl;
}

int main() {
  // 最初にやること
  prep_init();
  cout << "READY" << endl;
  // ゲーム設定の入力
  first_init();
  // ターン情報
  for (int t=1; t<=T; t++) {
    turn_init();
    if (debug_time) {
      auto startTime = chrono::system_clock::now();
      turn_output();
      auto endTime = chrono::system_clock::now();
      auto timeSpan = endTime - startTime;
      cerr << "unknown: "
           << chrono::duration_cast<chrono::milliseconds>(timeSpan).count()
           << endl;
    } else {
      turn_output();
    }
  }
}
