/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package grid_manager;

import java.net.InetAddress;

/**
 *
 * @author dcoss
 */
public class HostInfo {

    public HostInfo()
    {
        hostname = "127.0.0.1";
        port = grid_manager_utils.DEFAULT_PORT;
        hash = "";
    }
    public HostInfo(String newHostname, String newPort, String newHash)
    {
        hostname = newHostname;
        port = newPort;
        hash = newHash;
    }

    public String getHostname(){return hostname;}
    public void setHostname(String newHostname){hostname = newHostname;}

    public String getPort(){return port;}
    public void setPort(String newPort){port = newPort;}

    public String getHash(){return hash;}
    public void setHash(String newHash){ hash = newHash;}

    public String toString()
    {
        if(port == null || port.equals(""))
            port = grid_manager_utils.DEFAULT_PORT;
        return hostname + ":" + port;
    }

    public static boolean compare(String hostname, InetAddress inet)
    {
        if(hostname == null || inet == null)
            return false;
        
        return hostname.equals(inet.getHostAddress())
                || hostname.equals(inet.getHostName());
    }


    private String hostname,port,hash;

}
