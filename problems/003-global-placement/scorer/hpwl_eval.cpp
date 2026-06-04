// hpwl_eval — 用題目官方預編譯函式庫計算一份 .gp.pl 解答的 HPWL。
//
// readBookshelfFormat 的第二參數可指定替代的 placement (.pl) 檔，
// 因此載入「題目 .aux + 模型輸出的 .gp.pl」後呼叫 computeHpwl()，
// 即與官方 hw4 自報的 HPWL 完全一致（同一份 obj/Placement.o）。
//
// 編譯需連結 ../../reference/obj/*.o（題目提供之 Linux 預編譯庫），故僅能於 Linux 編譯執行。
//
// 用法： ./hpwl_eval <.aux> <.gp.pl>
// 輸出： 一行 HPWL 整數（無法讀取時以非零 exit code 結束並印錯誤到 stderr）

#include "Wrapper.hpp"
#include <cstdio>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <.aux> <.gp.pl>\n", argv[0]);
        return 2;
    }
    wrapper::Placement placement;
    placement.readBookshelfFormat(argv[1], argv[2]); // 第二參數：用此 .gp.pl 覆蓋初始位置
    printf("%.0f\n", placement.computeHpwl());
    return 0;
}
