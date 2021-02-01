import java.io.*;
import java.util.*;
import java.nio.file.Files;
import java.security.*;
import javax.crypto.spec.*;

import javax.crypto.*;

public class Locker {
	public static final int GCM_IV_LENGTH = 12;
	public static final int GCM_TAG_LENGTH = 16;

	private static String getFileExtension(File file) {
        String fileName = file.getName();
        if(fileName.lastIndexOf(".") != -1 && fileName.lastIndexOf(".") != 0)
        return fileName.substring(fileName.lastIndexOf(".")+1);
        else return "";
  }

	// Encrypt AES key for lock/unlock then print it to a file
	private static void encryptAESKey(String path, Key secretKey, Key publicKey) throws Exception {
		Cipher cipher = Cipher.getInstance("RSA");
		cipher.init(Cipher.ENCRYPT_MODE, publicKey);

		cipher.update(secretKey.getEncoded());
		byte[] cipherText = cipher.doFinal();

		// Printing to file
		FileOutputStream out = new FileOutputStream(new File(path + "/keyfile"));
		out.write(cipherText);
		out.close();
	}

	// generate AES key 256 byte
	private static SecretKey generateAESKey() throws NoSuchAlgorithmException {
		KeyGenerator keyGen = KeyGenerator.getInstance("AES");
		keyGen.init(256);
		SecretKey secretKey = keyGen.generateKey();

		return secretKey;
	}

	private static void signAESFile(String path, Key key) throws Exception {
		File file = new File(path + "/keyfile");
		byte[] data = Files.readAllBytes(file.toPath());

		Signature sign = Signature.getInstance("SHA256withRSA");
		sign.initSign((PrivateKey) key);
		sign.update(data);

		// generate signature
		byte[] signature = sign.sign();
		FileOutputStream out = new FileOutputStream(path + "/keyfile.sig");
		out.write(signature);
		out.close();
	}

	private static void encryptFile(String filePath, SecretKey secretKey) throws Exception {
		File file = new File(filePath);
		FileInputStream fileInputStream = null;
		byte[] inputFile = new byte[(int) file.length()];
		fileInputStream = new FileInputStream(file);
		fileInputStream.read(inputFile);
		fileInputStream.close();

		// Get Cipher Instance
		Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");

		// Create SecretKeySpec
		SecretKeySpec keySpec = new SecretKeySpec(secretKey.getEncoded(), "AES");

		// Create Nonce
		byte[] IV = new byte[GCM_IV_LENGTH];
		SecureRandom random = new SecureRandom();
		random.nextBytes(IV);

		// Create gcmParameterSpec
		GCMParameterSpec gcmParameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH * 8, IV);

		// Initialize Cipher for ENCRYPT_MODE
		cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmParameterSpec);

		// Encrypt
		byte[] cipherText = cipher.doFinal(inputFile);

		// Write Nonce to File
		FileOutputStream out = new FileOutputStream(new File(filePath + ".IV"));
		out.write(IV);
		out.close();

		// Write to Encrypted File
		out = new FileOutputStream(new File(filePath));
		out.write(cipherText);
		out.close();
	}

	// Encrypt a folder, traverse to each file then encrypt it
	public static void encryptDirectory(String directory, SecretKey secretKey) throws Exception {
		Queue<File> folderQueue = new LinkedList<>();
		folderQueue.add(new File(directory));

		while (!folderQueue.isEmpty()) {
			File folder = folderQueue.poll();
			File[] fileList = folder.listFiles();

			for(File file: fileList) {  // Iterate through the folder
				String ext = getFileExtension(file);
				if (!ext.equals("IV") && !ext.equals("sig") && !file.getName().equals("keyfile")) { // ignore keyfile, nonce, and keyfile.sig
					if (file.isFile()) // is normal filee
						encryptFile(file.getPath(), secretKey);
					else  // is directory
						folderQueue.add(file);
				}
			}
		}
	}

	public static void main(String[] args) throws Exception {

		// MISSING ARGUMENTS, BUT CAN DO THAT LATER

		// TESTER
		//String subject = "abc", pubPath = "public.txt", privPath = "private.txt", directory = "";
		String subject = "";
		String pubPath = "";
		String privPath = "";
		String directory = "";

		if (args.length != 8) {
			System.out.println("Usage: Locker -d <directory> -p <public key> -r <private key> -s <subject>");
			return;
		}
		if (args[0].equals("-d") && args[2].equals("-p") && args[4].equals("-r") && args[6].equals("-s")) {
			directory = args[1];
			pubPath = args[3];
			privPath = args[5];
			subject = args[7];
		} else {
			System.out.println("Usage: Locker -d <directory> -p <public key> -r <private key> -s <subject>");
			return;
		}

		PublicKey publicKey = KeyLoader.getPublicKey(subject, pubPath);
		if (publicKey == null) {
			System.out.println("Wrong subject");
			return;
		}
		PrivateKey privateKey = KeyLoader.getPrivateKey(privPath);

		SecretKey secretKey = generateAESKey(); // get AES key for encrypting
		encryptAESKey(directory, secretKey, publicKey); // encrypt the AES key using public key, print to file

		signAESFile(directory, privateKey);

		// LOCK|Encrypt THE FOLDER
		encryptDirectory(directory, secretKey);
	}
}
