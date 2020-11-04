package org.toontown.teapop;

/**
 * Authentication for use with teapop server
 * This interface is provided for reference because teapop does not check the interface,
 * it checks for the methods with correct signature in the authentication object
 */

public interface Authentication {

  /**
   * Called by teapop server to authenticate user
   *
   * @param userid userid of account to be authenticated
   * @param domain domain of account to be authenticated
   * @param password password to be authenticated
   * @param apopstr server APOP string to be used in APOP authentication
   * @param isapop indicate to use APOP authentication
   *
   * @return object that implements the User interface
   */
  public Object doPOPAuth(String userid,
                          String domain,
                          String password,
                          String apopstr,
                          boolean isapop);

}
