<?php
function valueToXML($value){
    if (mb_strpos($value,'<')!==false || mb_strpos($value,'&')!==false) {
      $value="<![CDATA[$value]]>";
    }
    return $value;
  }
function parseHeader($block)
{
return '<h'.$block->data->level.'>'.  $block->data->text   .'</h'.$block->data->level.'>';
}
function parseCode($block)
{
return '<pre><code class="language-'.$block->data->languageCode.' hljs">'.$block->data->code   .'</code></pre>';
}
function parseParagraph($block)
{
return '<p>'.$block->data->text   .'</p>';
}
function parseTable($block)
{
unset($return, $line, $cell);
$return = '<div class="table__wrap mb-8">
<table class="table">
<tbody>';
foreach ($block->data->content as $line) {
$return .= '<tr>';
foreach ($line as $cell) {
$return .= '<td class="table__cell"><div class="table__area">';
$return .= $cell;
$return .= '</div></td>';
}
$return .= '</tr>';
}
$return .= '</tbody></table></div>';
return $return;
}
function parseList($block)
{
unset($tag, $return, $item);
if ($block->data->style == 'ordered'){
$tag = 'ol';
}elseif ($block->data->style == 'unordered'){
$tag = 'ul';
}
$return = '<'.$tag.'>';
foreach ($block->data->items as $item) {
$return .= '<li>'.$item.'</li>';
}
$return .= '</'.$tag.'>';
return $return;
}
function parseImage($block)
{
unset($captiontitle, $captionalt);
if ($block->data->caption){
$captiontitle = '<p>'.$block->data->caption.'</p>';
$captionalt = $block->data->caption;
}
return '<div class="embedded_image" contenteditable="false" data-layout="default">
<img alt="'.$captionalt.'" src="'.$block->data->file->url.'">'.$captiontitle.'
</div>';
}
function echofeeddata($resultfeedmysqli, $returnfirstupdated){
unset($return, $publishing_date);
$author_id = $resultfeedmysqli['author_id'];
$sqlfeed_author = mysqli_query($conn, "SELECT `fullname` FROM `authors` WHERE `id` = '.$author_id.'");
$resultfeed_author = mysqli_fetch_assoc($sqlfeed_author);
//date
$publishing_date = date("r", strtotime($resultfeedmysqli['publishing_date']));
//date
if ($returnfirstupdated === true){
$return .= '<updated>'.$publishing_date.'</updated>';
}
$return .= '
<entry>
<title>
'.valueToXML($resultfeedmysqli['article_title']).'
</title>
<link rel="alternate" href="https://'.$_SERVER[HTTP_HOST].'/'.$resultfeedmysqli['nameaddress'].'"/>
<id>https://'.$_SERVER[HTTP_HOST].'/'.$resultfeedmysqli['nameaddress'].'</id>';
if ($resultfeed_author['fullname']){
$return .= '<author>
<name>
'.valueToXML($resultfeed_author['fullname']).'
</name>
</author>';
}
$content_array_feed = json_decode($resultfeedmysqli['content']);
if (is_object($content_array_feed)){
foreach ($content_array_feed->blocks as $block) {
switch ($block->type) {
case 'code':
$content_feed .= parseCode($block);
break;
case 'paragraph':
$content_feed .= parseParagraph($block);
break;
case 'header':
$content_feed .= parseHeader($block);
break;
case 'image':
$content_feed .= parseImage($block);
break;
case 'list':
$content_feed .= parseList($block);
break;
case 'table':
$content_feed .= parseTable($block);
break;
default:
// code...
break;
}
}
}
$content_feed = html_entity_decode($content_feed);
$return .= '<summary type="html">
'.valueToXML($content_feed).'
</summary>
<updated>'.$publishing_date.'</updated>
</entry>';
return $return;
}
echo '<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <id>https://'.$_SERVER[HTTP_HOST].'/feed</id>
  <link href="https://'.$_SERVER[HTTP_HOST].'/feed"/>
  <title>
    <![CDATA[ The Unreal Blog ]]>
  </title>
  <description/>
  <language/>'; ?>
  <?php
$sqlfeed_posts = mysqli_query($conn, "SELECT `article_title`, `nameaddress`, `publishing_date`, `author_id`, `content` FROM `articles` ORDER BY `publishing_date` DESC");
if ($sqlfeed_posts){
if(mysqli_num_rows($sqlfeed_posts) > 0){
$resultfeed_posts = mysqli_fetch_assoc($sqlfeed_posts);
echo echofeeddata($resultfeed_posts, true);
while($resultfeed_posts = mysqli_fetch_assoc($sqlfeed_posts)){
echo echofeeddata($resultfeed_posts, false);
}
}
}
?>
</feed>
