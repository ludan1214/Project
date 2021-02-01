5271 Programming assignment 2

Setup:
> make

Cleanup:
> make clean

Running the program:

// Generate the Keyfiles
java Keygen -s <subject> -pub <public key file path> -priv <private key file path>

// Lock the Directory
java Locker -d <directory to lock> -p <public key file path> -r <private key file path> -s <subject>

// Unlock the Directory
java Unlocker -d <directory to lock> -p <public key file path> -r <private key file path> -s <subject>
