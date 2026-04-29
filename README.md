
      CRYPTON ADVANCED GUI - CODE EXPLANATION GUIDE      
=========================================================
This document is designed to help you explain the inner workings
of the Crypton Advanced GUI project during your presentation.

---------------------------------------------------------
1. OVERALL ARCHITECTURE & FRAMEWORKS USED
---------------------------------------------------------
Why Dear ImGui over standard Windows Forms / WPF / Qt?
- "Dear ImGui" is an Immediate Mode GUI framework. It was chosen
  because it is incredibly lightweight, renders extremely fast 
  using hardware acceleration (OpenGL2), and gives complete pixel-level
  control over the UI. 
- Unlike standard WinForms which look outdated, ImGui allowed us to
  create a fully custom, borderless "Glassmorphic" transparent 
  window with real-time 60FPS animations (like the glowing Tron snakes 
  and background orbs) without heavy CPU usage.
- The UI binds directly to the native Win32 API to handle things
  like window creation, file dialogs (Browse button), and Drag-and-Drop
  functionality.

---------------------------------------------------------
2. THE GRAPHICS & ANIMATION (advanced_gui_main.cpp)
---------------------------------------------------------
How do the background animations work?
- The animations are handled in `DrawTronEffects()`.
- ImGui provides an `ImDrawList` which lets us draw primitive shapes
  directly to the screen. 
- The background "Orbs" use sine and cosine mathematical functions 
  (e.g., `sinf(time)`) to calculate their positions continuously, giving 
  them a natural, floating "wobble" effect.
- The "Snakes" tracing the border use a perimeter calculation function 
  (`GetPerimeterPos`). Based on the elapsed time, we draw overlapping 
  line segments with decreasing alpha (opacity) to create a glowing comet tail.

---------------------------------------------------------
3. MULTITHREADING & NON-BLOCKING UI
---------------------------------------------------------
Why does the UI stay responsive while encrypting large files?
- If encryption was done on the main thread, the window would freeze
  (showing "Not Responding" in Windows) until the file finished.
- To solve this, we used native Windows Threading (`CreateThread`).
- When you click "ENCRYPT", the `DoEncrypt()` function packages all the 
  data (paths, password) into an `OperationData` struct and passes it 
  to a background thread (`OperationThread`). 
- The main thread continues running at 60 FPS (keeping animations smooth), 
  and the background thread safely updates the global status variables 
  (g_statusType, g_statusMsg) when the encryption completes.

---------------------------------------------------------
4. CRYPTOGRAPHY CORE (encryptor.hpp)
---------------------------------------------------------
What algorithms are we using?
We use two main cryptographic libraries (both are header-only, making 
compilation easy without complex DLL dependencies):
1. 'plusaes.hpp' for AES-CBC 256-bit encryption (Advanced Encryption Standard).
   - AES is the industry standard for symmetric encryption.
   - We use CBC (Cipher Block Chaining) mode, meaning each block of data is 
     XORed with the previous encrypted block, adding an extra layer of security.
     This requires an IV (Initialization Vector), which we generate randomly.

2. 'picosha2.h' for SHA-256 Hashing.
   - Used for Key Derivation. AES needs a 256-bit (32-byte) key. Since user 
     passwords vary in length, we hash the password with SHA-256 to guarantee
     a perfectly sized 256-bit key.

How does the "Fast Pass" Wrong Password detection work?
- In the legacy format, if you entered a wrong password, the software would try 
  to decrypt the whole file, resulting in garbage data.
- The Advanced GUI implements a modern Secure File Format.
- During encryption, we generate a random 16-byte "Salt". We combine this salt 
  with the password and hash it using SHA-256. We store this Hash (Verifier)
  at the very beginning of the encrypted file.
- During decryption, we read the salt, combine it with the entered password, 
  hash it, and compare it to the stored Verifier. If they don't match, we 
  instantly reject the password WITHOUT attempting to decrypt the whole file.
  This is extremely secure and prevents the user from wasting time.

---------------------------------------------------------
5. INTERESTING SYNTAX EXAMPLES TO POINT OUT
---------------------------------------------------------
- Lambda Functions: In the UI code (around line 515), we use C++ lambda 
  functions `auto cPos=[&](float p)->ImVec2 { ... }`. This allows us to define 
  a quick helper function directly inside another function to keep the code clean.
- Bitwise Operators: When validating file paths, we check `(attr & FILE_ATTRIBUTE_DIRECTORY)`. 
  This uses bitwise AND to check if the specific "Directory" bit is set in the 
  Windows file attributes integer.

---------------------------------------------------------
6. POSSIBLE FACULTY QUESTIONS & ANSWERS
---------------------------------------------------------
Q: Why did you use C++ instead of Python/Java?
A: C++ provides direct memory management and native Win32 API access, resulting 
   in significantly faster file processing times for large encryption tasks, and 
   a much smaller, standalone executable footprint without needing a JVM or interpreter.

Q: Is the password stored anywhere?
A: No. The password is only kept in memory while typing. Once encryption/decryption 
   starts, the memory buffer `g_password` is immediately wiped using `memset(g_password, 0, ...)`. 
   The encrypted file only stores a salted hash of the password, meaning the original 
   password can never be reverse-engineered from the file.

Q: What happens if I rename an .enc file?
A: The software securely encrypts the original file extension inside the file metadata 
   before the main content. When decrypting, it reads this internal metadata to safely 
   restore the exact original file type (e.g., .pdf, .docx), regardless of what the 
   encrypted file was renamed to.
