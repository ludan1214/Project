import java.io.*;
import java.security.*;
import java.util.Arrays;

class Keygen {
    public static void writeKeyToFile(String path, String header, byte[] contents)
            throws java.io.IOException {
        File f = new File(path);
        if (f.getParent() == null) {
        	f.createNewFile();
        } else {
        	f.getParentFile().mkdirs();
        	f.createNewFile();
        }
        FileWriter out = new FileWriter(f);

        // Set bytes content into list of numbers
    	String stringContent = Arrays.toString(contents);
    	stringContent = stringContent.substring(1, stringContent.length() - 1);
    	stringContent = stringContent.replaceAll(",", "");

        if (header.equals("")) {
        	out.write(stringContent);
        } else {
        	out.write(header);
        	out.write(stringContent);

        }
        out.close();
    }


    public static void main(String[] args) {
        String subject = "", pubPath = "", privPath = "", header = "";

        // Argument parsing needs some improvement, but it works for now
        if (args.length != 6) {
            System.out.println("Usage: Keygen -s <subject> -pub <public key file> -priv <private key file>");
            return;
        }
        if (args[0].equals("-s") && args[2].equals("-pub") && args[4].equals("-priv")) {
            subject = args[1];
            pubPath = args[3];
            privPath = args[5];
        } else {
            System.out.println("Usage: Keygen -s <subject> -pub <public key file> -priv <private key file>");
            return;
        }

        /* Generate an RSA signature */
        try {
            KeyPairGenerator keyGen = KeyPairGenerator.getInstance("RSA");
            keyGen.initialize(2048);

            //Generate public and private key
            KeyPair keyPair = keyGen.generateKeyPair();
            PublicKey pubKey = keyPair.getPublic();
            PrivateKey privKey = keyPair.getPrivate();

            // Format the public certificate header (contains subject/algorithm)
            header += subject + "\n" + pubKey.getAlgorithm() + "\n";

            // Generate private key file
            writeKeyToFile(privPath, "", privKey.getEncoded());

            // Generate public key file
            writeKeyToFile(pubPath, header, pubKey.getEncoded());

        } catch (Exception e) {
            System.err.println("Caught exception " + e.toString());
        }
    }
}
