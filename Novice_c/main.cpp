#include <Novice.h>
#include <math.h>
#include <process.h>
#include <mmsystem.h>
#include <fstream>

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "winmm.lib")

using namespace std;

DWORD WINAPI Threadfunc(void*);

struct POS {
    int x;
    int y;
};
POS pos1P, pos2P, old_pos2P;
RECT rect;
SOCKET sWait;
bool bSocket = false;
HWND hwMain;

const char kWindowTitle[] = "KAMATA ENGINEクライアント";

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    Vector2 center;
    float radius;
} Circle;

// キー入力結果を受け取る箱
Circle a, b;
Vector2 center = { 100, 100 };
char keys[256] = { 0 };
char preKeys[256] = { 0 };
int color = RED;

int clrear = 0;
int timer = 0;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WSADATA wdData;
    static HANDLE hThread;
    static DWORD dwID;

    // ライブラリの初期化
    Novice::Initialize(kWindowTitle, 1280, 720);

    // ウィンドウハンドルの取得
    hwMain = GetActiveWindow();  // GetDesktopWindow()の代わりにGetActiveWindow()を使います

    // 白い球
    a.center.x = 400;
    a.center.y = 400;
    a.radius = 100;

    // 赤い球
    b.center.x = 200;
    b.center.y = 200;
    b.radius = 50;

    // winsock初期化
    WSAStartup(MAKEWORD(2, 0), &wdData);

    // データを送受信処理をスレッド（WinMainの流れに関係なく動作する処理の流れ）として生成
    hThread = (HANDLE)CreateThread(NULL, 0, &Threadfunc, (LPVOID)&a, 0, &dwID);

    // ウィンドウの×ボタンが押されるまでループ
    while (Novice::ProcessMessage() == 0) {
        // フレームの開始
        Novice::BeginFrame();

        // キー入力を受け取る
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

        if (keys[DIK_UP] != 0) {
            a.center.y -= 5;
        }
        if (keys[DIK_DOWN] != 0) {
            a.center.y += 5;
        }
        if (keys[DIK_RIGHT] != 0) {
            a.center.x += 5;
        }
        if (keys[DIK_LEFT] != 0) {
            a.center.x -= 5;
        }


        float distance =
            sqrtf((float)pow((double)a.center.x - (double)b.center.x, 2) +
                (float)pow((double)a.center.y - (double)b.center.y, 2));

        if (distance <= a.radius + b.radius) {
            color = BLUE;
        }
        else color = RED;

        if (color == BLUE) {
            clrear = 1;
        }
        if (timer >= 3600) {
            clrear = 2;
        }

        timer++;

        ///
        /// ↓描画処理ここから
        ///
        if (clrear == 0) {
            Novice::DrawEllipse((int)a.center.x, (int)a.center.y, (int)a.radius, (int)a.radius, 0.0f, WHITE, kFillModeSolid);
            Novice::DrawEllipse((int)b.center.x, (int)b.center.y, (int)b.radius, (int)b.radius, 0.0f, color, kFillModeSolid);
           
            Novice::ScreenPrintf(0, 0, "%d", timer / 60);
        }
        else if (clrear == 1) {
            Novice::ScreenPrintf(0, 0, "win");
        }
        else if (clrear == 2) {
            Novice::ScreenPrintf(0, 0, "lose");
        }

        ///
        /// ↑描画処理ここまで
        ///
        Novice::EndFrame();

        // ESCキーが押されたらループを抜ける
        if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
            break;
        }
    }

    // ライブラリの終了
    Novice::Finalize();

    // winsock終了
    WSACleanup();

    return 0;
}

DWORD WINAPI Threadfunc(void*) {
    SOCKET sConnect;
    SOCKADDR_IN saConnect;

    char addr[20]; // IPアドレス用配列

    // ファイル読み込み (#include <fstream> が必要)
    std::ifstream ifs("ip.txt");
    if (!ifs) {
        SetWindowText(hwMain, L"IPアドレスファイルが開けませんでした");
        return 1;
    }
    ifs.getline(addr, sizeof(addr));

    WORD wPort = 8000;

    // Winsockの初期化確認
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (wsaResult != 0) {
        SetWindowText(hwMain, L"WSAStartupエラー");
        return 1;
    }

    // ソケット作成
    sConnect = socket(PF_INET, SOCK_STREAM, 0);
    if (sConnect == INVALID_SOCKET) {
        SetWindowText(hwMain, L"ソケット作成エラー");
        WSACleanup();
        return 1;
    }

    // コネクト用SOCKADDR_IN変数を初期化
    ZeroMemory(&saConnect, sizeof(SOCKADDR_IN));
    saConnect.sin_family = AF_INET;
    saConnect.sin_addr.s_addr = inet_addr(addr);
    saConnect.sin_port = htons(wPort);

    // サーバに接続要求
    if (connect(sConnect, (sockaddr*)(&saConnect), sizeof(saConnect)) == SOCKET_ERROR) {
        int errCode = WSAGetLastError();  // エラーコードを取得
        char errMessage[256];
        snprintf(errMessage, sizeof(errMessage), "接続エラー: %d", errCode);
        SetWindowText(hwMain, L"接続エラー");

        // エラーコードを表示（デバッグ用）
        printf("接続エラー: %d\n", errCode);

        closesocket(sConnect);
        WSACleanup();
        return 1;
    }

    SetWindowText(hwMain, L"送信開始");

    while (1) {
        int nRcv;

        // 2Pの座標情報を送信
        send(sConnect, (const char*)&a, sizeof(Circle), 0);

        // サーバから1P座標情報を取得
        nRcv = recv(sConnect, (char*)&b, sizeof(Circle), 0);

        if (nRcv == SOCKET_ERROR) {
            int errCode = WSAGetLastError();
            printf("受信エラー: %d\n", errCode);
            SetWindowText(hwMain, L"通信エラー");
            break;
        }

        // 通信が正常であれば、座標更新
        if (nRcv > 0) {
            // キャラクターの座標更新など、画面描画処理を行う
        }
    }

    closesocket(sConnect);
    WSACleanup();

    return 0;
}

#if 0
// 通信スレッド関数
DWORD WINAPI threadfunc(void* px) {

	SOCKET sConnect;
	struct sockaddr_in saConnect, saLocal;
	DWORD dwAddr;
	// 送信バッファ
	Circle* lpBuf = new Circle;

	// サーバプログラムを実行するPCのIPアドレスを設定
//	const char* addr = "192.168.0.22";
	char addr[17];
	WORD wPort = 8000;

	ifstream ifs("ip.txt");
	ifs.getline(addr, 16);

	dwAddr = inet_addr(addr);

	sConnect = socket(PF_INET, SOCK_STREAM, 0);

	ZeroMemory(&saConnect, sizeof(sockaddr_in));
	ZeroMemory(&saLocal, sizeof(sockaddr_in));

	saLocal.sin_family = AF_INET;
	saLocal.sin_addr.s_addr = INADDR_ANY;
	saLocal.sin_port = 0;

	bind(sConnect, (LPSOCKADDR)&saLocal, sizeof(saLocal));

	saConnect.sin_family = AF_INET;
	saConnect.sin_addr.s_addr = dwAddr;
	saConnect.sin_port = htons(wPort);

	// サーバーに接続
	if (connect(sConnect, (sockaddr*)(&saConnect), sizeof(saConnect)) == SOCKET_ERROR)
	{
		//		SetWindowText(hwMain, "接続エラー");

		closesocket(sConnect);

		WSACleanup();

		return 1;
	}

	SetWindowText(hwMain, L"送信開始\r\n");

	// 通信スタート
	send(sConnect, (char*)&a, sizeof(Circle), 0);

	while (1)
	{
		int     nRcv;

		// 送られてくる1Pの座標情報を退避
//		recv_circle = pos1P;

		// サーバから1P座標情報を取得
		nRcv = recv(sConnect, (char*)&b, sizeof(Circle), 0);

		// 通信が途絶えたらループ終了
		if (nRcv == SOCKET_ERROR)
		{
			break;
		}

		// 2Pの座標情報を送信
		send(sConnect, (const char*)&a, sizeof(Circle), 0);
	}

	closesocket(sConnect);

	// 送信データバッファ解放
	delete lpBuf;

	return 0;
}
#endif