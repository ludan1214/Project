import java.io.*;
import java.security.*;
import java.security.spec.*;

public class KeyLoader {
	// This helps converting string format from file to correct byte data
	public static EncodedKeySpec getSpec(String line, boolean isPublic) {
		String[] token = line.split(" ");

		// Get public key
		byte[] encodedKey = new byte[token.length];
		for (int i = 0; i < token.length; i++) {
			encodedKey[i] = (byte) Integer.parseInt(token[i]);
		}

		EncodedKeySpec spec;
		if (isPublic)
			spec = new X509EncodedKeySpec(encodedKey);
		else
			spec = new PKCS8EncodedKeySpec(encodedKey);
		return spec;
	}

	// get public key from file
	public static PublicKey getPublicKey(String subject, String pubPath) throws Exception {
		// Read public key
		BufferedReader in = new BufferedReader(new FileReader(pubPath));
		String line;
		line = in.readLine(); // subject
		if (!line.equals(subject)) {
			in.close();
			return null;
		}

		line = in.readLine(); // algorithm
		line = in.readLine(); // bit-value for public key
		in.close();

		EncodedKeySpec spec = getSpec(line, true);
		KeyFactory kf = KeyFactory.getInstance("RSA");
		PublicKey publicKey = kf.generatePublic(spec); // generate public key

		return publicKey;
	}

	// Get private key from file
	public static PrivateKey getPrivateKey(String privPath) throws Exception {
		BufferedReader in = new BufferedReader(new FileReader(privPath));
		String line;
		line = in.readLine(); // bit-value for private key
		in.close();

		EncodedKeySpec spec = getSpec(line, false);
		KeyFactory kf = KeyFactory.getInstance("RSA");
		PrivateKey privateKey = kf.generatePrivate(spec); // generate public key

		return privateKey;
	}
}
