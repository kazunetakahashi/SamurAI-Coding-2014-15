#include <iostream>
#include <algorithm>
#include <cassert>
// #include <random>
using namespace std;

bool debug = false;

// 大域変数・定数
const int T = 9; // 全ターン数
const int P = 4; // プレイヤー(大名)数
const int N = 6; // 領主の数
const int remain[10] = {23, 9, 8, 7, 6, 5, 8, 7, 6, 5}; // 見込み投票数
const int maxpt = 6;
const int minpt = 3;
const int noonpeople = 5;
const int nightpeople = 2;
int A[N]; // 領主の兵力
int total_score; // 兵力の合計値
int minimum_score; // 合格最低点
int priority[N];
int x_now=0, y_now=0; // 現在の戦略
int top_lord=2, bottom_lord=3;
int now_amari = 0;

// 戦略変数
double ai_num = 14; // 0から15まで。動かす。
double ai_last = 4; // 0から15まで。動かす。
double alpha, beta;

double K_top[2]; // トップになるための係数
double K_bottom[2]; // 最下位にならないための係数
double C_top[2]; // トップになるための定数
double C_bottom[2]; // 最下位にならないための定数
double minimum_rate; // 合格最低点/兵力の合計値

// ターンでの変数
int turn; // 現在のターン
bool isnoon; // 昼か？
int B[N][P]; // B[n][p] = プレイヤーpの領主nへの親密度
int R[N]; // 自分の本当の投票数
int W[N]; // 前の休日までの、明らかになっていない夜の交渉回数(自分を除く)
int people; // 出力数
int L[noonpeople]; // 出力
bool voted_tops = false; // そのターンに「取りに行く領主」に投票したか
bool voted_bottoms = false; // そのターンに「落とさない領主」に投票したか

// 5ターン目終了時のみ点数計算
double points[P];
const double epsilon = 0.00001;

void getpoint() {
  fill(points, points+P, 0);
  for (int i=0; i<N; i++) {
    int max_v = -1;
    int min_v = 10000;
    for (int j=0; j<P; j++) {
      if (max_v < B[i][j]) max_v = B[i][j];
      if (min_v > B[i][j]) min_v = B[i][j];
    }
    int max_p = 0;
    int min_p = 0;
    for (int j=0; j<P; j++) {
      if (max_v == B[i][j]) max_p++;
      if (min_v == B[i][j]) min_p++;
    }
    double max_gp = ((double)A[i])/max_p;
    double min_gp = ((double)A[i])/min_p;
    for (int j=0; j<P; j++) {
      if (max_v == B[i][j]) points[j] += max_gp;
      if (min_v == B[i][j]) points[j] -= min_gp;
    }    
  }
  /*
  for (int j=0; j<P; j++) {
    cerr << points[j] << " ";
  }
  cerr << endl;
  */
}

bool isnowtop() {
  getpoint();
  for (int i=1; i<P; i++) {
    if (points[0] + epsilon < points[i]) return false;
  }
  return true;
}

// 係数の調整
void keisu(double* p, double x, double y) {
  *p = alpha * x + beta * y;
}

void senryaku_hensu(int i) {
  if (i == 1) {
    if (!(isnowtop())) {
      ai_num = ai_last;
    }
  }
  alpha = 1 - ai_num/10;
  beta = 1 - alpha;
  keisu(&K_top[i], 1.35, 1.4);
  keisu(&K_bottom[i], 1.35, 1.3);
  keisu(&C_top[i], 1, 1.3);
  keisu(&C_bottom[i], 0.5, 1.2);
  keisu(&minimum_rate, 0.25, 0.1);  
}


// 初期化・入力関連
void first_init() {
  int x;
  cin >> x;
  cin >> x;
  cin >> x;
  total_score = 0;
  for (int i=0; i<N; i++) {
    cin >> A[i];
    total_score += A[i];
  }
  fill(W, W+N, 0);
  // priorityの確定
  bool used[N];
  fill(used, used+N, false);
  // priority[2]の確定
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=0; j<N; j++) {
      if (A[j] == i) {
	priority[2] = j;
	used[j] = true;
	goto EXIT_OF_PRIORITY_2;
      }
    }
  }
 EXIT_OF_PRIORITY_2:
  // priority[0][1]の確定
  int count = 0;
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=N-1; j>=0; j--) {
      if (!used[j] && A[j] == i) {
	priority[count++] = j;
	used[j] = true;
	if (count == 2) {
	  goto EXIT_OF_PRIORITY_01;
	}
      }
    }
  }
 EXIT_OF_PRIORITY_01:
  // priority[3][4][5]の確定
  count = 3;
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=N-1; j>=0; j--) {
      if (!used[j] && A[j] == i) {
	priority[count++] = j;
	used[j] = true;
	if (count == 5) {
	  goto EXIT_OF_PRIORITY_345;
	}
      }
    }
  }  
 EXIT_OF_PRIORITY_345:
  // amari = priority[3];
  return;
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
  }
  if (isnoon) {
    for (int i=0; i<N; i++) {
      int w;
      cin >> w;
      W[i] += w;
    }
  }
  // 係数の設定
  if (turn == 1) {
    senryaku_hensu(0);
  } else if (turn == 6) {
    senryaku_hensu(1);
  }
}

// 判断基準の関数たち
int expected_votes(int lord, bool istop, int daimyo) {
  int m = B[lord][daimyo];
  double K = K_top[0];
  double Const = C_top[0];
  int be_af = ( (turn <= 5) ? 0 : 1 );
  if (istop) {
    K = K_top[be_af];
    Const = C_top[be_af];
  } else {
    K = K_bottom[be_af];
    Const = C_bottom[be_af];
  }
  return m + K * W[lord] + Const;
}

int need_votes(int lord, bool istop) {
  int ret = expected_votes(lord, istop, 1);
  for (int i=1; i<P; i++) {
    if (istop) {
      ret = max(ret, expected_votes(lord, istop, i));
    } else {
      ret = min(ret, expected_votes(lord, istop, i));
    }
  }
  return ret;
}

bool hantei(int lord, bool istop) {
  return R[lord] >= need_votes(lord, istop);
}


bool isgood(int lord_priority) {
  int lord = priority[lord_priority];
  if (lord_priority < top_lord) {
    return hantei(lord, true);
  } else {
    return hantei(lord, false);
  }
}

void determine_minimum_score() {
  minimum_score = (int) (minimum_rate * total_score);
}

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

// 戦略の決定と実行
void determine_priority() {
  determine_minimum_score();
  int max_waste = -100000;
  int max_x = x_now;
  int max_y = y_now;
  for (int x=0; x< (1 << N); x++) {
    for (int y=0; y< (1 << N); y++) {
      if (isvalid(x, y) && score(x, y) > minimum_score) {
        // そもそも残りのターンで実現可能か？
        int n_vote = 0;
        for (int i=0; i<N; i++) {
          if ( (x >> i) & 1 ) {
            n_vote += max(need_votes(i, true) - R[i], 0);
          } else if ( (y >> i) & 1 ) {
            n_vote += max(need_votes(i, false) - R[i], 0);            
          } else {
            // 投票する必要はない
          }
        }
        if (n_vote > remain[turn]) continue;
        // 相手に捨てさせる票数の計算
        int waste = 0;
        for (int i=0; i<N; i++) {
          // Bを破壊しないように、整列用の配列を用意
          pair<int, int> sorted_B[P];
          for (int j=1; j<P; j++) {
            sorted_B[j-1] = make_pair(B[i][j], j);
          }
          sort(sorted_B, sorted_B+P-1);
          reverse(sorted_B, sorted_B+P-1);
          if ( (x >> i) & 1 ) {
            // 捨てる票はない
          } else if ( (y >> i) & 1 ) {
            int myvotes = need_votes(i, false);
            // 1位の人と2位の人の票が捨てさせる票
            for (int j=0; j<P-2; j++) {
              waste += max(0,
                           expected_votes(i, true, sorted_B[j].second)
                           *(P-2-j)/(P-2)
                           + expected_votes(i, false, sorted_B[j].second)
                           *j/(P-2)
                           - myvotes);
            }
          } else {
            int myvotes = R[i];
            // 全員の票が捨てさせる票
            for (int j=0; j<P-1; j++) {
              waste += max(0,
                           expected_votes(i, true, sorted_B[j].second)
                           *(P-2-j)/(P-2)
                           + expected_votes(i, false, sorted_B[j].second)
                           *j/(P-2)
                           - myvotes);
            }
          }          
        }
        // 今までの中で最も良い戦略なら採用
        if (waste > max_waste) {
          max_waste = waste;
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
    cerr << "max_waste: " << max_waste << endl;
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

void determine_L() {
  if (turn == 1) {
    // random_device rd;
    // mt19937 mt(rd());
    for (int i=0; i<5; i++) {
      L[i] = priority[i];
      // L[i] = priority[mt()%6];
    }
  } else {
    int now_c = 0;
    int now_p = 0;
    // 戦略に基づき割り振る
    while (now_p < top_lord+bottom_lord && now_c < people) {
      if (!isgood(now_p)) {
	int lord = priority[now_p];
	L[now_c++] = lord;
	if (now_p < top_lord) {
          voted_tops = true;
        } else {
          voted_bottoms = true;
        }
	R[lord] += ((isnoon) ? 1 : 2);
      } else {
	now_p++;
      }
    }
    // 枠が余ったら
    while (now_c < people) {
      now_amari %= top_lord+bottom_lord;
      L[now_c++] = priority[(now_amari++)%(top_lord+bottom_lord)];
      if (debug) cerr << "amari" << endl;
    }
  }
}

// 出力
void turn_output() {
  if (turn >= 2) determine_priority();
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

/*
void circle() {
  if (voted_tops) {
    swap(priority[0], priority[1]);
    voted_tops = false;
  }
  if (voted_bottoms) {
    swap(priority[2], priority[3]);
    swap(priority[3], priority[4]);
    voted_bottoms = false;
  }
}
*/

int main() {
  cout << "READY" << endl;
  // ゲーム設定の入力
  first_init();
  // ターン情報
  for (int t=1; t<=T; t++) {
    turn_init();
    turn_output();
    /* circle(); */
  }
}
