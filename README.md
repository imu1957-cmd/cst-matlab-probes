# CST / MATLAB 环境探测脚本

一组用来**确认本机接口能不能用**的探测脚本，目的是在动手写 MCP 服务之前，先把 CST Studio Suite 2024 与 MATLAB 的 Python 接口摸清楚：有哪些类、哪些方法、参数怎么传、返回值长什么样。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `_probe_cst_api.py` | 导入 `cst` 包，列出 `interface` 模块下 `DesignEnvironment` / `Project` / `Model3D` 的公开成员 |
| `_probe_cst_docs.py` | 打印 `new_project` / `new_mws` / `new_ems` 等构造函数与 `open_project` 的 docstring |
| `_probe_cst_license.py` | 端到端最小闭环：起 DesignEnvironment → 建 MWS 工程 → 写一段 VBA 到 history → 保存 → 关闭（顺带验证 license 能否正常获取） |
| `_probe_cst_project.py` | 打印 `Project` / `Model3D` 各方法的 docstring，并测试 `save()` 与 `save(path)` 两种保存方式 |
| `_probe_cst_results.py` | 探测 `cst.results`：打开工程文件、取 3D 结果、读取结果树（离线读结果不占 license） |
| `_probe_cst_results2.py` | 列出 `ResultModule` 的成员，测试 `get_all_result_trees()` / `get_result_tree()` 的返回结构 |
| `_probe_cst_results3.py` | 打印 `ResultModule.get_tree_items` / `get_result_item` / `get_run_ids` 与 `ResultItem.get_data` 等方法的 docstring |
| `_probe_cst_results4.py` | 列出 `DesignEnvironment.StartMode` 的枚举值，并对两个真实工程读 `0D/1D` 结果条目及其数量 |
| `_probe_matlab_engine.py` | 导入 `matlab.engine`、启动引擎、`eval("1+1")`、读取版本号、退出并计时 |

## 运行环境

- Windows + **CST Studio Suite 2024**（Python 库在 `D:\CST-2024\AMD64\python_cst_libraries`）
- **MATLAB**，且已安装 Python 引擎（`D:\MATLAB\extern\engines\python`）
- Python 3

脚本里的路径都是本机绝对路径，换机器跑要改 `sys.path.insert(...)` 那一行。

## 用法

```bash
python _probe_cst_api.py
python _probe_matlab_engine.py
```

直接运行，看终端输出即可。每个脚本都以 `print` 汇报结果，失败时打印异常类型和截断后的错误信息，成功结尾会打印 `DONE`。

> 注意：`_probe_cst_license.py` 会真实启动 CST 并占用 license；`_probe_cst_results*.py` 只是**离线读**已保存的 `.cst`，不占 license。
