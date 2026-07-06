#!/bin/bash
# test_unit.sh — 分片打包逻辑验证，无需任何硬件
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "============================================"
echo "  第一层：分片打包单元测试"
echo "  无需硬件，纯逻辑验证"
echo "============================================"
echo ""

cd "$PROJECT_DIR"

if [ ! -f "test_fragmentation.py" ]; then
    echo "❌ 找不到 test_fragmentation.py"
    echo "   请确认文件在: $PROJECT_DIR/test_fragmentation.py"
    exit 1
fi

python3 test_fragmentation.py
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ 单元测试全部通过"
else
    echo "❌ 单元测试失败，请先修复再进行下一层测试"
fi

exit $EXIT_CODE