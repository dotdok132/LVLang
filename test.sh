OUTPUT=$(./lvlc "01C10502050401C2050205FF" 2>/dev/null | awk '/^Output: /{flag=1; sub(/^Output: /, ""); printf "%s", $0; next} /^\[VM Halted\]/{flag=0; next} flag{printf "\\n%s", $0}')
echo "GOT: '$OUTPUT'"
