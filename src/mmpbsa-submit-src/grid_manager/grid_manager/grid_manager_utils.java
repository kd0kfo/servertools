
package grid_manager;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.ParseException;

/**
 * Provides various functions and constants used by the grid manager program.
 * 
 * @author dcoss
 *
 */
public class grid_manager_utils
{
    //customizable default values.
    public static String DEFAULT_PORT = "31416";
    public static String APPNAME = "St Jude Grid Manager";
    public static String GRID_NAME = "St Jude Computing Grid";
    public static String CLIENT_HOSTNAME = "localhost";
    public static String PROJECT_URL = "http://stjudeathome2.stjude.org/stjudeathome";
    public static String[] version = {"0","1","4"};


    public static enum FORMAT_TYPE{TEXT,TIME}
    
    //pseudo enum for use in formating.
    public static enum RESULT_DATA_TYPES{fraction_done,elapsed_time,current_cpu_time,estimated_cpu_time_remaining,exit_status,active_task_state}

    public static String byteToHex(byte[] input)
    {
        StringBuffer buf = new StringBuffer();
        for (int i = 0; i < input.length; i++) {
            int halfbyte = (input[i] >>> 4) & 0x0F;
            int two_halfs = 0;
            do {
                if ((0 <= halfbyte) && (halfbyte <= 9))
                    buf.append((char) ('0' + halfbyte));
                else
                    buf.append((char) ('a' + (halfbyte - 10)));
                halfbyte = input[i] & 0x0F;
            } while(two_halfs++ < 1);
        }
        return buf.toString();
    }

    public static String getGuiRPCAuthHash(String dtime, String password)
    {
        String toDigest = dtime + password;
        return getGuiRPCAuthHash(toDigest);
    }

    public static String getGuiRPCAuthHash(String toHash)
    {
        MessageDigest md;
        try{
            md = MessageDigest.getInstance("md5");
            byte[] hash = md.digest(toHash.getBytes());
            return byteToHex(hash);

        }
        catch(java.security.NoSuchAlgorithmException nsae)
        {
            return "";
        }
    }

    public static void asynchronousWarning(javax.swing.JFrame frame)
    {
        warn(frame,"The host data is not synchronized. Remove host from list"
                    + " and re-enter information.");
    }

    public static void warn(javax.swing.JFrame frame,String message){warn(frame,message,"");}

    public static void warn(javax.swing.JFrame frame,String message, String title)
    {
        javax.swing.JOptionPane.showMessageDialog(frame,message,title,javax.swing.JOptionPane.ERROR_MESSAGE);
    }

    public static String toTimeString(String timeInSeconds)
    {
        String returnMe = "";
        if(timeInSeconds == null)
            return returnMe;
        try{
            double seconds = Double.parseDouble(timeInSeconds);
            double previous = 0;
            Integer tmp;
            if(seconds > 24*3600)
            {
                tmp = (int) java.lang.Math.floor(seconds/(24*3600));
                returnMe = tmp.toString() + " days";
                previous = tmp.intValue()*24*3600;
            }
            if(seconds > 3600)
            {
                tmp = (int) java.lang.Math.floor((seconds-previous)/(3600));
                returnMe += " " + tmp.toString() + " hrs";
                previous += tmp*3600;
            }
            if(seconds > 60)
            {
                tmp = (int) java.lang.Math.floor((seconds-previous)/60);
                returnMe += " " + tmp.toString() + " mins";
                previous += tmp*60;
            }
            if(seconds - previous > 0)
            {
                tmp = (int)(seconds-previous);
                returnMe += " "+ tmp.toString() + " secs";
            }
        }
        catch(java.lang.NumberFormatException nfe)
        {
            javax.swing.JOptionPane.showMessageDialog(null,"Not a valid integer: " + timeInSeconds);
            return timeInSeconds;
        }
        return returnMe;
    }

    public static String taskStateString(String data)
    {
        try
        {
            switch(Integer.parseInt(data))
            {
                case 1:
                    return "Running";
                case 0: case 2: case 9:
                    return "Suspended";
                default:
                    return "Unknown";
            }
        }
        catch(java.lang.NumberFormatException nfe)
        {
            return data;
        }
    }

    public static String taskExitStatusString(String data)
    {
        if(data.trim().equals("0"))
            return "Active";
        
        try
        {
            int status = Integer.parseInt(data);
            switch(status)
            {
                case -197:
                    return "Aborted by user";
                case 0:
                    return "Successful Finish";
                default:
                    break;
            }
        }
        catch(java.lang.NumberFormatException nfe)
        {
        }
        return "Ended with status of " + data;
    }

    public static String formatResultData(String dataType, String data)
    {
        if(dataType == null || data == null)
            return "";
        try
        {
            switch(RESULT_DATA_TYPES.valueOf(dataType))
            {
                case current_cpu_time: case elapsed_time: case estimated_cpu_time_remaining:
                    return toTimeString(data);
                case active_task_state: 
                    return taskStateString(data);
                case fraction_done:
                    return new Integer((int) (100 * Double.parseDouble(data))).toString() + "%";
                case exit_status:
                    return taskExitStatusString(data);
                default:
                    return data;
            }
        }
        catch(java.lang.IllegalArgumentException iae)
        {
            
        }

        return data;
    }


    public static String processMessage(RPCMap msg)
    {
        String returnMe = "";
        java.util.Date timeStamp;
        java.text.SimpleDateFormat fmt;
        boolean hasProject = msg.containsKey("project") && msg.get("project").length() != 0;
        boolean hasTime = msg.containsKey("time");
        if(hasProject || hasTime)
        {
            returnMe += "[";
            if(msg.containsKey("project"))
                returnMe += msg.get("project");
            if(hasProject && hasTime)
                returnMe += " ";
            if(hasTime)
            {
                try
                {
                    fmt = new java.text.SimpleDateFormat("s");
                    timeStamp  = fmt.parse(msg.get("time"));
                    returnMe += timeStamp.toString();
                }
                catch(java.text.ParseException pe)
                {
                    System.out.println(pe.getMessage());
                }
            }
            returnMe += "] ";
        }
        if(msg.containsKey("body"))
            returnMe += msg.get("body");

        return returnMe;
    }
    
}
