for f in $(ls ../traces/); do
    echo "Running $f"
    ./mdriver -af ../traces/$f
    echo ""
done

echo "==================="
echo "Overall Performance"
echo "==================="
./mdriver -av
