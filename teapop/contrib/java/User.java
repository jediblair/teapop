package org.toontown.teapop;

/**
 * User data for use with teapop server
 * This interface is provided for reference because teapop does not check the interface,
 * it checks for the methods with correct signature in the user object
 */

public interface User {

  /**
   * Get path to user mailbox
   *
   * @return path to mailbox relative to domain directory
   */
  public String getMailbox();

}
