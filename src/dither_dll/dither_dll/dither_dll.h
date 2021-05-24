// 以下の ifdef ブロックは、DLL からのエクスポートを容易にするマクロを作成するための
// 一般的な方法です。この DLL 内のすべてのファイルは、コマンド ラインで定義された DITHERDLL_EXPORTS
// シンボルを使用してコンパイルされます。このシンボルは、この DLL を使用するプロジェクトでは定義できません。
// ソースファイルがこのファイルを含んでいる他のプロジェクトは、
// DITHERDLL_API 関数を DLL からインポートされたと見なすのに対し、この DLL は、このマクロで定義された
// シンボルをエクスポートされたと見なします。
#ifdef DITHERDLL_EXPORTS
#define DITHERDLL_API __declspec(dllexport)
#else
#define DITHERDLL_API __declspec(dllimport)
#endif


extern "C" {
	DITHERDLL_API int luaopen_OrderedDithering(lua_State *L);
}