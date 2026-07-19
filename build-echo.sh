#!/usr/bin/env bash
#
# build-echo.sh — 一键编译 tin 库 + echo 示例（WSL2 / Linux）
#
# 用法:
#   ./build-echo.sh           # Debug 构建（默认）
#   ./build-echo.sh release   # Release 构建
#   ./build-echo.sh clean     # 清理后重新构建
#
# 产物:
#   build/bin/echo            # echo 服务器可执行文件
#
# 环境要求:
#   - WSL2 或 Linux
#   - clang++（推荐通过 Homebrew 安装: brew install llvm）
#   - cmake >= 3.10
#   - ninja
#   - 如果使用 Homebrew clang，脚本会自动加载 brew shellenv

set -euo pipefail

# ---------------------------------------------------------------------------
# 颜色输出
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ---------------------------------------------------------------------------
# 参数解析
# ---------------------------------------------------------------------------
BUILD_TYPE="Debug"
DO_CLEAN=0

for arg in "$@"; do
  case "$arg" in
    release|Release|RELEASE)
      BUILD_TYPE="Release"
      ;;
    clean)
      DO_CLEAN=1
      ;;
    *)
      fail "未知参数: $arg\n用法: $0 [release|clean]"
      ;;
  esac
done

# ---------------------------------------------------------------------------
# 定位项目根目录
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

info "项目根目录: $SCRIPT_DIR"
info "构建类型:   $BUILD_TYPE"

# ---------------------------------------------------------------------------
# 加载 Homebrew 环境（WSL2 上 clang/cmake 可能通过 Homebrew 安装）
# ---------------------------------------------------------------------------
if [ -f /home/linuxbrew/.linuxbrew/bin/brew ]; then
  eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"
  info "已加载 Homebrew 环境"
fi

# ---------------------------------------------------------------------------
# 检查依赖工具
# ---------------------------------------------------------------------------
for cmd in cmake ninja clang++; do
  if ! command -v "$cmd" &>/dev/null; then
    fail "未找到 '$cmd'，请先安装。\n  Homebrew: brew install $cmd"
  fi
done
ok "依赖工具检查通过: $(cmake --version | head -1), $(ninja --version), $(clang++ --version | head -1)"

# ---------------------------------------------------------------------------
# 构建目录
# ---------------------------------------------------------------------------
BUILD_DIR="build"
if [ "$DO_CLEAN" -eq 1 ]; then
  info "清理旧构建目录: $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# ---------------------------------------------------------------------------
# CMake 配置
# ---------------------------------------------------------------------------
info "CMake 配置中..."
cmake -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DBASE_JUMBO_BUILD=ON \
  .. 2>&1 | tail -5

ok "CMake 配置完成"

# ---------------------------------------------------------------------------
# 编译 echo
# ---------------------------------------------------------------------------
info "编译 tin 库 + echo 示例..."
START_TIME=$SECONDS

ninja tin echo 2>&1 | tail -10

ELAPSED=$((SECONDS - START_TIME))
ok "编译完成，耗时 ${ELAPSED}s"

# ---------------------------------------------------------------------------
# 验证产物
# ---------------------------------------------------------------------------
ECHO_BIN="bin/echo"
if [ -f "$ECHO_BIN" ]; then
  FILE_SIZE=$(du -h "$ECHO_BIN" | cut -f1)
  ok "echo 二进制: $BUILD_DIR/$ECHO_BIN ($FILE_SIZE)"
else
  fail "echo 二进制未找到: $BUILD_DIR/$ECHO_BIN"
fi

# ---------------------------------------------------------------------------
# 使用说明
# ---------------------------------------------------------------------------
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN} 编译成功！${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "运行 echo 服务器:"
echo -e "  ${BLUE}./build/bin/echo${NC}"
echo ""
echo -e "echo 服务器默认监听端口 2222，可用 nc 测试:"
echo -e "  ${BLUE}nc 127.0.0.1 2222${NC}"
echo ""
echo -e "其他选项:"
echo -e "  ${YELLOW}./build-echo.sh release${NC}  # Release 构建"
echo -e "  ${YELLOW}./build-echo.sh clean${NC}    # 清理后重新构建"
echo ""
