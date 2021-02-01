import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.*;
import java.util.LinkedList;
import java.util.Queue;

import javax.crypto.spec.SecretKeySpec;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.*;

//https://stackoverflow.com/questions/11410770/load-rsa-public-key-from-file
//https://mkyong.com/java/how-to-convert-file-into-an-array-of-bytes/
public class Unlocker {
	public static final int GCM_IV_LENGTH = 12;
	public static final int GCM_TAG_LENGTH = 16;

	private static String getFileExtension(File file) {
        String fileName = file.getName();
        if(fileName.lastIndexOf(".") != -1 && fileName.lastIndexOf(".") != 0)
        return fileName.substring(fileName.lastIndexOf(".")+1);
        else return "";
    }

	private static boolean verifySignature(String path, PublicKey publicKey) throws Exception {
		Signature sign = Signature.getInstance("SHA256withRSA");
		sign.initVerify(publicKey);
		byte[] signature = Files.readAllBytes(Paths.get(path + "/keyfile.sig")); // get signature
		byte[] data = Files.readAllBytes(Paths.get(path + "/keyfile")); // get original data that was signed
		sign.update(data);

		return sign.verify(signature);
	}

	private static SecretKey decryptAESKey(String filePath, PrivateKey privateKey) throws Exception {
		File keyFile = new File(filePath+"/keyfile");
		byte[] inputFile = new byte[(int) keyFile.length()];
		FileInputStream fileInputStream = new FileInputStream(keyFile);
		fileInputStream.read(inputFile);
		fileInputStream.close();

		Cipher decriptCipher = Cipher.getInstance("RSA");
		decriptCipher.init(Cipher.DECRYPT_MODE, privateKey);
		byte[] decryptedBytes = decriptCipher.doFinal(inputFile);

		// Delete keyfile
		keyFile.delete();

		// Return the KEY
		SecretKey secretKey = new SecretKeySpec(decryptedBytes, 0, decryptedBytes.length, "AES");
		return secretKey;
	}

	private static void decryptFile(String filePath, SecretKey secretKey) throws Exception {
		// Get File
		File keyFile = new File(filePath);
		byte[] inputFile = new byte[(int) keyFile.length()];
		FileInputStream fileInputStream = new FileInputStream(keyFile);
		fileInputStream.read(inputFile);
		fileInputStream.close();

		// Get Nonce
		File nonceFile = new File(filePath + ".IV");
		byte[] IV = new byte[GCM_IV_LENGTH];
		fileInputStream = new FileInputStream(nonceFile);
		fileInputStream.read(IV);
		fileInputStream.close();

		// Get Cipher Instance
		Cipher decriptCipher = Cipher.getInstance("AES/GCM/NoPadding");

		// Create SecretKeySpec
		SecretKeySpec keySpec = new SecretKeySpec(secretKey.getEncoded(), "AES");

		// Create GCMParameterSpec
		GCMParameterSpec gcmParameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH * 8, IV);

		// Initialize Cipher for DECRYPT_MODE
		decriptCipher.init(Cipher.DECRYPT_MODE, keySpec, gcmParameterSpec);
		byte[] decryptedBytes = decriptCipher.doFinal(inputFile);

		// Write out File
		FileOutputStream out = new FileOutputStream(new File(filePath));
		out.write(decryptedBytes);
		out.close();

		// delete Nonce
		nonceFile.delete();
	}

	// Decrypt a folder, traverse to each file then decrypt it
	public static void decryptDirectory(String directory, SecretKey secretKey) throws Exception {
		Queue<File> folderQueue = new LinkedList<>();
		folderQueue.add(new File(directory));

		while (!folderQueue.isEmpty()) {
			File folder = folderQueue.poll();
			File[] fileList = folder.listFiles();

			for(File file: fileList) {  // Iterate through the folder
				// need to ignore the .IV files
				String ext = getFileExtension(file);
				if (!ext.equals("IV") && !ext.equals("sig") && !file.getName().equals("keyfile")) { // ignore keyfile, nonce, and keyfile.sig
					if (file.isFile()) // is normal file
						decryptFile(file.getPath(), secretKey);
					else  // is directory
						folderQueue.add(file);
				}
			}
		}
	}

	public static void main(String[] args) throws Exception {
		// TESTER
		//String subject = "abc", pubPath = "public.txt", privPath = "private.txt", directory = "";

		String subject = "";
		String pubPath = "";
		String privPath = "";
		String directory = "";

		if (args.length != 8) {
			System.out.println("Usage: Unlocker -d <directory> -p <public key> -r <private key> -s <subject>");
			return;
		}
		if (args[0].equals("-d") && args[2].equals("-p") && args[4].equals("-r") && args[6].equals("-s")) {
			directory = args[1];
			pubPath = args[3];
			privPath = args[5];
			subject = args[7];
		} else {
			System.out.println("Usage: Unlocker -d <directory> -p <public key> -r <private key> -s <subject>");
			return;
		}

		PublicKey publicKey = KeyLoader.getPublicKey(subject, pubPath);
		if (publicKey == null) {
			System.out.println("Wrong subject");
			return;
		}
		PrivateKey privateKey = KeyLoader.getPrivateKey(privPath); // verify signature

		boolean verify = verifySignature(directory, publicKey);
		if (!verify) {
			System.out.println("Cannot verify keys, please make sure to use correct one");
		} else {
			// Verified, Delete the .sig file
			File sigFile = new File(directory+"/keyfile.sig");
			sigFile.delete();
			// FETCH AES KEY FROM keyfile
			SecretKey secretKey = decryptAESKey(directory, privateKey);

			// Unlock Directory
			decryptDirectory(directory, secretKey);
			// Delete keyfile and keyfile.sig
		}

	}
}
