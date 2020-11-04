package org.toontown.teapop;


/**
 * This is a sample Authentication Class for use with teapop
 *
 * In a real usage, is recommended to use one pool for jdbc connections
 */

import java.util.Properties;
import java.io.InputStream;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.DriverManager;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class SampleAuth implements Authentication, User {

  private static String dbURL;
  private static String dbUserid;
  private static String dbPassword;
  private static String dbQuery;

  static {
      Properties prop = new Properties();
      try {
          /**
           * The directory with teapop.properties must be defined on classpath
           */
          InputStream is = ClassLoader.getSystemResourceAsStream(
              "teapop.properties");
          prop.load(is);
          is.close();
      } catch (Exception e) {
          System.err.println("Error loading teapop.properties");
          System.exit(0);
      }
      dbURL = prop.getProperty("teapop.dburl");
      dbUserid = prop.getProperty("teapop.dbuserid");
      dbPassword = prop.getProperty("teapop.dbpassword");
      dbQuery = prop.getProperty("teapop.dbquery");
  }

  private static MessageDigest md;

  static {
      try {
          md = MessageDigest.getInstance("MD5");
      } catch (NoSuchAlgorithmException e) {
          md = null;
      }
  }



  /**
   * teapop will create an instance for each autentication that must be made,
   * then we can use the same object to implement the 2 interfaces.
   * If you already have routines to get userinfo, you can implement the User
   * interface in yout user class
   */
  private String mailbox;

  public String getMailbox() {
      return mailbox;
  };

  /**
   * Its recommended to use the connection via a Pool Driver, or create an static
   * connection object if your database driver support multiple statements in a single connection
   */
  private Connection c = null;

  public static String calcAPOP(String password, String apopstr) {
      if (md == null) {
          /**
           * Can't do APOP
           */
          return "";
      }
      password = apopstr + password;
      byte[] d = md.digest(password.getBytes());
      // Convert MD5 byte array to Hex
      password = "";
      for (int x = 0; x < d.length; x++) {
          /**
           * Convert byte (signed) to int without signal
           */
          int i = d[x];
          if (i < 0) {
              i = i + 256;
          }
          i = i + 256;
          password = password + Integer.toHexString(i).substring(1).toLowerCase();
      }
      return password;
  }


  public Object doPOPAuth(String userid,
                          String domain,
                          String password,
                          String apopstr,
                          boolean isapop) {
      boolean ok = false;
      try {
          c = DriverManager.getConnection(dbURL, dbUserid, dbPassword);
          PreparedStatement pst = c.prepareStatement(dbQuery);
          pst.setString(1, userid);
          pst.setString(2, domain);
          ResultSet rs = pst.executeQuery();
          if (rs.next()) {
              String dbpassword = rs.getString(1);
              if (isapop) {
                  dbpassword = calcAPOP(dbpassword, apopstr);
              }
              ok = dbpassword.equals(password);
              if (ok) {
                  mailbox = "/" + userid + "/";
              }
          }
          rs.close();
          pst.close();
      } catch (Exception e) {
          /**
           * An unexpected error ocurred, please write it to file, syslog or something else
           * DOES NOT USE System.err or System.out here, if you make it and you are using
           * inetd or xinetd the output will be sent to POP client
           * I recommend to use System.SetErr and System.SetOut to make the redirection
           */
      }
      if (c != null) {
          try { c.close(); } catch (Exception e) {};
      }
      if (ok) {
          return this;
      } else {
          return null;
      }
  }

}
