/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package grid_manager;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.xml.sax.InputSource;
import java.io.StringReader;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;
import org.xml.sax.SAXException;
/**
 *
 * @author dcoss
 */
public class RPCMap extends java.util.HashMap<String, String>{

    public static String[] getImportantNodes(){
        return null;
    }

    private static String getTextValue(org.w3c.dom.Element ele, String tagName)
    {
		String textVal = null;
		NodeList nl = ele.getElementsByTagName(tagName);
		if(nl != null && nl.getLength() > 0) {
			Element el = (Element)nl.item(0);
                        if(el.hasChildNodes())
                            textVal = el.getFirstChild().getNodeValue();
		}

		return textVal;
	}

    private static boolean isImportant(String tag, String[] important_tags)
    {
        if(important_tags == null || important_tags.length == 0)
            return true;
        for(int i = 0;i<important_tags.length;i++)
            if(important_tags[i].equals(tag))
                return true;

        return false;
    }

    public static java.util.Vector<RPCMap> parse(String topMostTag, String data, String[] task_info)
    {
        java.util.Vector<RPCMap> returnMe = new java.util.Vector<RPCMap>();
            
        try
        {
            DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
            DocumentBuilder db = dbf.newDocumentBuilder();
            InputSource data_source = new InputSource();
            data_source.setCharacterStream(new StringReader(data));
            Document doc = db.parse(data_source);
            doc.getDocumentElement().normalize();
            NodeList results = doc.getElementsByTagName(topMostTag);
            for(int i = 0;i<results.getLength();i++)
            {
                Node topNode = results.item(i);
                if(topNode.getNodeType() == Node.ELEMENT_NODE)
                {
                    Element result = (Element) topNode;
                    RPCMap currResult = new RPCMap();
                    NodeList resultData = result.getElementsByTagName("*");
                    for(int j = 0;j<resultData.getLength();j++)
                    {
                        Node stuff = resultData.item(j).getFirstChild();
                        String name = resultData.item(j).getNodeName();
                        if(name == null)
                            name = "null";
                        if(!isImportant(name,task_info))
                            continue;
                        String value = "";
                        if(stuff != null)
                        {
                            if(stuff.getNodeType() == Node.TEXT_NODE)
                                value = stuff.getNodeValue();
                            else
                                value = stuff.getNodeName();
                        }
                        if(value == null)
                            value = "";

                        currResult.put(name,value);
                    }
                    returnMe.add(currResult);
                }
            }
        }
        catch(java.io.IOException ioe)
        {
            System.err.println(ioe.getMessage());
            returnMe.clear();
            return returnMe;
        }
        catch (SAXException saxe)
        {
            System.err.println(saxe.getMessage());
            returnMe.clear();
            return returnMe;
        }
        catch (ParserConfigurationException pce)
        {
            System.err.println(pce.getMessage());
            returnMe.clear();
            return returnMe;
        }
        return returnMe;
    }

    public static java.util.Vector<RPCMap> parseOld(String topMostTag, String data, String[] task_info)
    {
        throw new java.lang.RuntimeException("Deprecated. Remove from source code.");
        /*DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        java.util.Vector<RPCMap> returnMe = new java.util.Vector<RPCMap>();

        try
        {
            DocumentBuilder db = dbf.newDocumentBuilder();
            InputSource data_source = new InputSource();
            data_source.setCharacterStream(new StringReader(data));
            Document doc = db.parse(data_source);
            NodeList results = doc.getElementsByTagName("boinc_gui_rpc_reply");
            if(results.getLength() == 0)
                return returnMe;

            results = ((Element) results.item(0)).getElementsByTagName(topMostTag);
            RPCMap curr = new RPCMap();
            Element currResult;
            
            for(int i = 0;i<results.getLength();i++)
            {
                curr.clear();
                currResult = (Element) results.item(i);
                if(task_info == null || task_info.length == 0)
                {
                    NodeList all = currResult.getElementsByTagName("result");
                    System.out.println("Results: " + Integer.toString(all.getLength()));

                    for(int j = 0;j<all.getLength();j++)
                    {
                        NodeList currNode =  ((Element) all.item(j)).getElementsByTagName("*");
                        for(int k = 0;k<currNode.getLength();k++)
                        {
                            String name = currNode.item(k).getNodeName();
                            String val = getTextValue((Element) all.item(j), name);
                            if(val == null)
                                val = "";
                            curr.put(name,val);
                        }
                    }
                }
                else
                {
                    for(int j = 0;j<task_info.length;j++)
                    {
                        curr.put(task_info[j],getTextValue(currResult,task_info[j]));
                    }
                }
                returnMe.add(curr);
            }
        }
        catch (ParserConfigurationException pce)
        {
            System.err.println(pce.getMessage());
            returnMe.clear();
        }
        catch(org.xml.sax.SAXException saxe)
        {
            System.err.println(saxe.getMessage());
            returnMe.clear();
        }
        catch(java.io.IOException ioe)
        {
            System.err.println(ioe.getMessage());
            returnMe.clear();
        }
        return returnMe;*/
    }

}
