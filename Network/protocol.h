#pragma once
constexpr int MAX_NAME = 100;
constexpr unsigned char C2S_LOGIN = 1;
constexpr unsigned char C2S_MOVE = 2;
constexpr unsigned char C2S_INPUT = 3;
constexpr unsigned char S2C_LOGIN_OK = 1;
constexpr unsigned char S2C_ADD_PLAYER = 2;
constexpr unsigned char S2C_MOVE_PLAYER = 3;
constexpr unsigned char S2C_REMOVE_PLAYER = 4;
constexpr unsigned char S2C_INPUT_PLAYER = 5;



constexpr int MAX_BUFFER = 1024;
constexpr short SERVER_PORT = 3500;
constexpr int WORLD_X_SIZE = 8;
constexpr int WORLD_Y_SIZE = 8;
constexpr int MAX_USER = 10;
#pragma pack(push, 1)

struct KeyInput
{
	HWND hWnd;
	UINT uMsg;
	WPARAM wParam;
	LPARAM lParam;
};

struct c2s_login {
	unsigned char size;
	unsigned char type;
	char name[MAX_NAME];
};

enum DIRECTION { D_N, D_S, D_W, D_E, D_NO };
struct c2s_move {
	unsigned char size;
	unsigned char type;
	DIRECTION dir;
};

struct c2s_input {
	unsigned char size;
	unsigned char type;
	KeyInput key;
};

#pragma pack(pop)

struct s2c_login_ok {
	unsigned char size;
	unsigned char type;
	float x, y, z;
	int id;
	bool isSpawn;
};

struct s2c_add_player {
	unsigned char size;
	unsigned char type;
	float x, y, z;
	int id;
	bool isAdd;
};

struct s2c_move_player {
	unsigned char size;
	unsigned char type;
	int id;
};

struct s2c_input_player {
	unsigned char size;
	unsigned char type;
	int id;
};

struct s2c_remove_player {
	unsigned char size;
	unsigned char type;
	int id;
};