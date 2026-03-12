# Huffman File Compressor

A high-performance C-based file compression utility that uses the **Huffman Coding Algorithm** to reduce file size. The program generates compressed files with the `.gidds` extension.

## 🚀 How to Run

### 1. Compilation
Use any C compiler (like GCC) to compile the source code:
```bash
gcc compressor.c -o compressor
```

### 2. Compression
To compress a file:
```bash
./compressor -c <filename>
```
*Output: `<filename>.gidds`*

### 3. Decompression
To restore the original file:
```bash
./compressor -d <filename>.gidds
```
*Output: `<filename>_out`*

---

## 🧠 The Huffman Algorithm

Huffman coding is a **lossless data compression** algorithm. The core idea is to assign variable-length binary codes to characters based on their frequency:
- **Frequent characters** get shorter codes (e.g., 'e' might be `01`).
- **Rare characters** get longer codes (e.g., 'z' might be `11010`).

By using fewer bits for common data, the overall file size is significantly reduced.

### Workflow in `compressor.c`:

1.  **Frequency Analysis**: The program first scans the entire input file to count the occurrences of every byte (0-255).
2.  **Building the Min-Heap**: Every unique character is placed into a Priority Queue (Min-Heap) based on its frequency.
3.  **Constructing the Huffman Tree**:
    - The two nodes with the smallest frequencies are popped from the heap.
    - A new "parent" node is created with a frequency equal to the sum of the two nodes.
    - This parent is pushed back into the heap.
    - This repeats until only one node (the **Root**) remains.
4.  **Generating Codes**: The tree is traversed. Going **Left** adds a `0` to the code, and going **Right** adds a `1`.
5.  **Bit Packing**: Since computers write in 8-bit bytes, the program uses a `BitWriter` buffer to group the variable-length codes into full bytes before writing them to the disk.
6.  **Header Storage**: The frequency table is stored inside the compressed file so the tree can be reconstructed perfectly during decompression.

---

## 📂 File Structure (.gidds)

The compressed file is structured as follows:
- **Magic Number**: `HUFF` (4 bytes) to identify the file type.
- **Table Size**: 2 bytes indicating the number of unique characters.
- **Frequency Table**: A list of character-frequency pairs.
- **Padding Info**: 1 byte indicating how many bits to ignore at the very end.
- **Compressed Data**: The actual stream of Huffman-coded bits.

---

## 💡 Features
- **Folder Independent**: Run the command on a file path, and the compressed version will be created in that same directory.
- **Binary Support**: Works on both text files and binary data.
- **Efficient Memory**: Uses a Min-Heap implementation for fast tree construction.
