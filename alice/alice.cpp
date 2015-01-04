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

// randomはメルセンヌ・ツイスターで作る
random_device rd;
mt19937 mt(rd());

int randmod(int m) {
  return mt()%m;
}

// 大域変数・定数
const int T = 9; // 全ターン数
const int P = 4; // プレイヤー(大名)数
const int N = 6; // 領主の数
const int remain_day[10] = {15, 15, 10, 10, 5, 5, 10, 10, 5, 5}; // 昼残り
const int remain_night[10] = {4, 4, 4, 2, 2, 0, 4, 2, 2, 0}; // 夜残り
int remain_votes[10]; // remain_day + remain_night * 2
const int noonpeople = 5;
const int nightpeople = 2;
int A[N]; // 領主の兵力
int total_score; // 兵力の合計値
int priority[N];
int x_now=0, y_now=0; // 現在の戦略
int top_lord=0, bottom_lord=0; // 1位を取る領主、3位を取る領主
int now_amari = 0;

// ターンでの変数
int turn; // 現在のターン
bool isnoon; // 昼か？
int B[N][P]; // B[n][p] = プレイヤーpの領主nへの親密度
int R[N]; // 自分の本当の投票数
int W[N]; // 前の休日までの、明らかになっていない夜の交渉回数(自分を除く)
int people; // 出力数
int L[noonpeople]; // 出力

// ランダムで方針を決定するためのもの
const int RD_turn[10] = {0, 0, 7500, 8500, 500, 5000,
                         3500, 4000, 500, 5000}; // RD = RD_turn[turn];
const int RD_M = 10000;
int RD;
int random_votes[T+1][RD_M][N][P];
int rv_n_first[T+1][RD_M][N];
int rv_n_third[T+1][RD_M][N];
int rv_n_first_max[T+1][N];
int rv_n_third_max[T+1][N];
int rv_n_first_min[T+1][N];
int rv_n_third_min[T+1][N];
const double minimum_rate = 0.05; // 合格最低ライン
double minimum_score; // 合格最低点

// 深さ優先探査で総当りするためのもの
typedef tuple<int, int*> future;
stack<future> St;
int L_prep[9];

// 点数計算用
double points_zenhan[P];
const double epsilon = 0.00001;

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
  // トップかどうかを判定する。
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

void determine_points_zenhan() { // 前半のポイントを計算する。
  fill(points_zenhan, points_zenhan+P, 0);
  getpoint(B, points_zenhan);
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
}

// 初期化・入力関連
void prep_init() {
  total_score = 0;
  fill(W, W+N, 0);
  // remain_votesを埋める
  for (int i=0; i<=T; i++) {
    remain_votes[i] = remain_day[i] + remain_night[i] * 2;
  }
  // points_zenhan[0]だけは初期化する。
  points_zenhan[0] = 0;
  // これからのことは全部ランダムで割り振る。
  // いつやってもいいので、冒頭にやっておく。
  // ルール上最初の待ち時間は5秒、各ターンの待ち時間は1秒。
  for (int t=1; t<=T; t++) {
    for (int l=0; l<RD_M; l++) {
      for (int j=1; j<P; j++) {
        for (int k=0; k<remain_votes[t]; k++) {
          int i = randmod(N);
          random_votes[t][l][i][j]++;
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
      cin >> B[i][j];
    }
  }
  for (int i=0; i<N; i++) {
    cin >> R[i];
  }
  if (turn == 6) {
    fill(W, W+P, 0);
    determine_points_zenhan();
  }
  if (isnoon) {
    for (int i=0; i<N; i++) {
      int w;
      cin >> w;
      W[i] += w;
    }
  }
  RD = RD_turn[turn];
}

// 相手の投票数をランダムで求める

void random_write(int t) {
  // 明らかになっているところ
  for (int i=0; i<N; i++) {
    random_votes[turn][t][i][0] = R[i];
    for (int j=1; j<P; j++) {
      random_votes[turn][t][i][j] += B[i][j];
    }
  }  
  // 夜の交渉回数をランダムで割り振る
  int WR[N];
  for (int i=0; i<N; i++) {
    WR[i] = W[i];
  }
  int done_night = remain_night[0] - remain_night[turn];
  for (int j=1; j<P; j++) {
    for (int k=0; k<done_night; k++) {
      while (true) {
        int i = randmod(N);
        if (WR[i] > 0) {
          random_votes[turn][t][i][j] += 2;
          WR[i]--;
          break;
        }
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
    rv_n_first[turn][t][i] = max(temp[P-2] - R[i] + 1, 0);
    rv_n_third[turn][t][i] = max(temp[0] - R[i] + 1, 0);
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
 
// 1, 2, 3, 6, 7 ターン目は、戦略で動く。

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
              random_votes[turn][t][i][0] = R[i] + rv_n_first[turn][t][i];
            } else if ( ((y >> i) & 1) == 1 ) {
              random_votes[turn][t][i][0] = R[i] + rv_n_third[turn][t][i];
            } else {
              random_votes[turn][t][i][0] = R[i];
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
}

// 4, 5, 8, 9 ターン目は、投票の総当りで動く。

int count_win(int* q) {
  int ans = 0;
  for (int t=0; t<RD; t++) {
    for (int i=0; i<N; i++) {
      random_votes[turn][t][i][0] = R[i];
    }
    for (int l=0; l<remain_votes[turn]; l++) {
      int i = q[l];
      random_votes[turn][t][i][0]++;
    }
    if (isnowtop(random_votes[turn][t])) {
      ans++;
    }
  }
  return ans;
}

void depth() {
  for (int t=0; t<RD; t++) {
    random_write(t);
  }
  int max_win = -100;
  int* Xp = (int*) malloc(sizeof (int) * 9);
  future F = make_tuple(0, Xp);
  St.push(F);
  while(!St.empty()) {
    int d = get<0>(St.top());
    int* p = get<1>(St.top());
    St.pop();
    if (d == 0) {
      for (int i=0; i<N; i++) {
        int* q = (int*) malloc(sizeof (int) * 9);
        q[d] = i;
        St.push(make_tuple(d+1, q));
      }
    } else if (d < remain_votes[turn]) {
      for (int i=p[d-1]; i<N; i++) {
        int* q = (int*) malloc(sizeof (int) * 9);
        for (int j=0; j<d; j++) {
          q[j] = p[j];
        }
        q[d] = i;
        St.push(make_tuple(d+1, q));  
      }
    } else {
      int win = count_win(p);
      if (max_win < win) {
        max_win = win;
        for (int i=0; i<remain_votes[turn]; i++) {
          L_prep[i] = p[i];
        }
      }
    }
    free(p);
  }
  if (debug) {
    cerr << "max_win: " << max_win << " of " << RD_turn[turn] << endl;
    for (int i=0; i<remain_votes[turn]; i++) {
      cerr << L_prep[i] << " ";
    }
    cerr << endl;
  }
}

// 実行
void determine_L() {
  if (turn == 1) {
    for (int i=0; i<5; i++) {
      L[i] = priority[i];
    }
  } else if (turn == 2 || turn == 3 || turn == 6 || turn == 7) {
    determine_priority();
    int now_c = 0;
    int now_p = 0;
    // 戦略に基づき割り振る
    int omomi = ((isnoon) ? 1 : 2);
    // 投票して必ず損がないもの
    int must_vote[N];
    int must_vote_c = 0;
    for (int i=0; i<top_lord; i++) {
      must_vote[i] = rv_n_first_min[turn][priority[i]];
      must_vote_c += must_vote[i]/omomi;
    }
    for (int i=top_lord; i<top_lord+bottom_lord; i++) {
      must_vote[i] = rv_n_third_min[turn][priority[i]];
      must_vote_c += must_vote[i]/omomi;
    }
    // 投票するべきもの
    int should_vote[N];
    int should_vote_c = 0;
    for (int i=0; i<top_lord; i++) {
      should_vote[i] = rv_n_first_max[turn][priority[i]];
      should_vote_c += should_vote[i]/omomi;
    }
    for (int i=top_lord; i<top_lord+bottom_lord; i++) {
      should_vote[i] = rv_n_third_max[turn][priority[i]];
      should_vote_c += should_vote[i]/omomi;
    }
    while (now_c < people && now_c < must_vote_c) {
      if (must_vote[now_p] >= omomi) {
	int lord = priority[now_p];
	L[now_c++] = lord;
	must_vote[now_p] -= omomi;
        should_vote[now_p] -= omomi;
      }
      now_p++;
      now_p %= (top_lord+bottom_lord);
    }
    while (now_c < people && now_c < should_vote_c) {
      if (should_vote[now_p] >= omomi) {
	int lord = priority[now_p];
	L[now_c++] = lord;
	should_vote[now_p] -= omomi;
      }
      now_p++;
      now_p %= (top_lord+bottom_lord);
    }
    // 枠が余ったら
    while (now_c < people) {
      now_amari %= top_lord+bottom_lord;
      L[now_c++] = priority[(now_amari++)%(top_lord+bottom_lord)];
      if (debug) cerr << "amari" << endl;
    }
  } else {
    depth();
    if (turn == 4 || turn == 8) {
      int temp[N];
      fill(temp, temp+N, 0);
      for (int i=0; i<remain_votes[turn]; i++) {
        temp[L_prep[i]]++; 
      }
      int temp_max = -1;
      for (int i=0; i<N; i++) {
        if (temp_max < temp[i]) temp_max = temp[i];
      }
      if (temp_max >= people * 2) {
        for (int i=0; i<N; i++) {
          if (temp_max == temp[i]) {
            for (int j=0; j<people; j++) {
              L[j] = i;
            }
            break;
          }
        }
      } else {
        int j=0;
        for (int i=0; i<N; i++) {
          if (temp[i] >= 2) {
            L[j++] = i;
            if (j == people) break;
          }
        }
      }
    } else { // turn == 5 || turn == 9
      for (int i=0; i<people; i++) {
        L[i] = L_prep[i];
      }
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
    if(!isnoon) {
      W[L[i]]--;
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
           << chrono::duration_cast<chrono::milliseconds>(timeSpan).count() * 3
           << endl;
    } else {
      turn_output();
    }
  }
}
