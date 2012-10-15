<?php
if(!isset($_POST['input']))
{
  echo "Need Input";
  exit();
}

$xml_parser = xml_parser_create();
//echo $_POST['input'];
xml_parse_into_struct($xml_parser,$_POST['input'],$vals,$index);

/*/
print_r($index);
echo "\n\n";
print_r($vals);/**/

$username = $vals[$index['NAME'][0]]['value'];
$passwd_hash = $vals[$index['PASSWORD_HASH'][0]]['value'];

if($passwd_hash == "0395277a1d4a891695b1c740acf35a86")
{
  print "<acct_mgr_reply>
    <name>St Jude Grid Manager Name</name>
    <message>Welcome to the grid!</message>
    <account>
       <url>http://stjudeathome.stjude.org</url>
       <authenticator>5d3af82f259773854aaf3f78f0b2e624</authenticator>
    </account>
    <rss_feeds>
       <rss_feed>
          <url>http://davecoss.com/RSS/rss.php</url>
          <poll_interval>60</poll_interval>
       </rss_feed>
    </rss_feeds>
</acct_mgr_reply>";

}
else
{
  echo "<error>
    <error_num>-206</error_num>
    <error_string>Wrong password: " . $passwd_hash . "</error_string>
</error>";
}

?>
