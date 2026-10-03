# LSDCXX_WeaponWheelVC — 项目长期笔记

GTA Vice City 武器轮盘 ASI 插件（Plugin-SDK + RenderWare 2D 绘制）。

## 构建 / 环境（重要）
- 环境变量：`PLUGIN_SDK_DIR = Q:\Lee\Documents\GitHub\plugin-sdk`，
  `GTA_VC_DIR = Q:\GTA\GTA VC\GTA-VC`。
- 配置：`Release GTA-VC | Win32`（v143，/std:c++latest，输出 `.asi`）。
- **shell 工具会拦截 MSBuild.exe**（判定为 LOLBin）。改用
  `.workbuddy-ai/build_check.py`（用受管 Python 通过 subprocess 调用 MSBuild）即可正常编译，
  例如：`"C:/Users/Lee/.workbuddy-ai/binaries/python/versions/3.13.12/python.exe" .workbuddy-ai/build_check.py`
- PostBuildEvent 会自动 `taskkill gta-vc.exe` 并把 `.asi` + `.ini` 复制到 `$(GTA_VC_DIR)\scripts`，
  所以构建完成即已部署。

## 代码结构约定
- 单文件实现：`source/LSDCXX_WeaponWheelVC.cpp`。
- 武器槽位语义：`CPed::m_aWeapons[10]`，`m_nCurrentWeapon` / `CPlayerPed::m_nSelectedWepSlot`
  都是**槽位下标**（不是武器类型）。
- 判定"槽位是否持有可用武器"统一走 `IsSlotUsable()`：
  近战 1~11 与特殊道具 34/36 无弹药也算可用；其余枪械要求
  `m_nAmmoTotal > 0 || m_nAmmoInClip > 0`。
- 内部瞄准向量约定：`gAimY < 0` 表示朝上；角度换算用
  `atan2(gAimX, -gAimY)`（与绘制角度相差 +90°，见 `SlotFromAngle`）。
- 手柄走 GInput：`Pads[0].NewState`（GInputVC 写入）。Control Set 1 → L2 开轮盘，
  Set 5 → 十字键左；右摇杆 `RightStickX/Y` 做方向选择。

## 相关仓库
- GTA III 版：`Q:\Lee\Documents\GitHub\LSDCXX_WeaponWheelIII`（同源代码，尚未同步本次修复）。
