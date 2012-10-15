/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package grid_manager;

import java.util.Vector;


/**
 *
 * @author dcoss
 */
public class ResultInfo extends java.util.HashMap<String, String>
{
    public ResultInfo(RPCMap map)
    {
        java.util.Iterator<String> it = map.keySet().iterator();
        String key;
        while(it.hasNext())
        {
            key = it.next();
            this.put(key, map.get(key));
        }
    }
    
    public static String[] getImportantNodes(){
        String[] returnMe = {"name","estimated_cpu_time_remaining"
            ,"project_url","fraction_done","current_cpu_time","active_task_state"
            ,"exit_status"};
        return returnMe;
    }

    public static java.util.Vector<ResultInfo> parse(String data)
    {
        return parse("result",data);
    }

    public static java.util.Vector<ResultInfo> parse(String topMostTag, String data)
    {
        java.util.Vector<RPCMap> maps = RPCMap.parse(topMostTag, data, getImportantNodes());
        java.util.Vector<ResultInfo> resultinfos = new java.util.Vector<ResultInfo>();
        
        java.util.Iterator<RPCMap> it = maps.iterator();
        while(it.hasNext())
        {
            resultinfos.add(new ResultInfo(it.next()));
        }

        return resultinfos;
    }

}
