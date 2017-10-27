for f in $(ls ../traces/); do
    echo "Running $f"
    ./mdriver -f ../traces/$f
    echo ""
done

echo "==================="
echo "Overall Performance"
echo "==================="
./mdriver
