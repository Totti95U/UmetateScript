#include <lua.hpp>

struct Pixel_RGBA {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
};

int alpha0(lua_State* L) {
	Pixel_RGBA* pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	int col = static_cast<int>(lua_tointeger(L, 4));

	//colをr, g, b 値に変換
	int r = (col >> 16) & 0xff;
	int g = (col >> 8) & 0xff;
	int b = (col >> 0) & 0xff;

	//colとピクセルの色の透明度による加重平均にする
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			double alpha = pixels[index].a / 255.0;
			pixels[index].r = static_cast<unsigned char>(pixels[index].r * alpha + r * (1 - alpha));
			pixels[index].g = static_cast<unsigned char>(pixels[index].g * alpha + g * (1 - alpha));
			pixels[index].b = static_cast<unsigned char>(pixels[index].b * alpha + b * (1 - alpha));
			pixels[index].a = 0xff;
		}
	}

	return 0;
}

static luaL_Reg functions[] = {
	{ "alpha0", alpha0 },
	{ nullptr, nullptr }
};

extern "C" {
	__declspec(dllexport) int luaopen_Umetate_DLL(lua_State* L) {
		luaL_register(L, "Umetate_DLL", functions);
		return 1;
	}
}