#ifndef STYLE_H
#define STYLE_H

#include <Arduino.h>

// CSS（<style>タグの中身）をPROGMEM（フラッシュメモリ）に格納
const char STYLE_CSS[] PROGMEM = R"rawliteral(
  body {
    font-family: Arial, sans-serif;
    text-align: center;
    background-color: #f0f0f0;
    margin: 0;
    padding: 20px;
  }
  h1 {
    color: #333;
    margin-bottom: 30px;
    border-bottom: 2px solid #ccc;
    padding-bottom: 10px;
  }
  .container {
    display: flex;
    justify-content: center;
    gap: 30px;
    flex-wrap: wrap;
  }
  .room {
    border: 1px solid #ccc;
    background-color: white;
    padding: 20px;
    width: 450px; /* 部屋の幅 */
    height: 600px; /* 高さを600pxに維持 */
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    position: relative;
  }
  .room-name {
    position: absolute;
    top: 10px;
    left: 50%;
    transform: translateX(-50%);
    font-weight: bold;
    font-size: 1.2em;
    color: #555;
  }
  .grid-container {
    display: grid;
    grid-template-rows: repeat(8, 50px); 
    gap: 10px;
    width: 100%;
    justify-items: center;
    padding-top: 40px; /* 部屋名との間隔 */
  }
  /* 2-301 は 3列グリッド */
  .room-301 .grid-container {
    grid-template-columns: repeat(3, 1fr); 
  }
  /* 2-302 は 5列グリッド */
  .room-302 .grid-container {
    grid-template-columns: repeat(5, 1fr); 
  }
  
  .grid-item {
    width: 80px; /* 四角のサイズ (幅) */
    height: 50px; /* 四角のサイズ (高さ) */
    border: 1px solid black;
    background-color: #eee;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: bold;
    font-size: 1.2em;
    color: #333;
  }
  /* 5列グリッド用に幅を調整 */
  .room-302 .grid-item {
    width: 60px; 
  }
  
  .highlighted {
    background-color: #ffcccc; /* ハイライト色 (薄い赤) */
    border: 1px solid red;
    color: #cc0000; /* ハイライト時の文字色 */
  }

  /* 2-301 の配置 (3列グリッド) (grid-area: row-start / col-start / row-end / col-end) */
  .room-301 .grid-item:nth-child(1) { grid-area: 1 / 1 / 2 / 2; }
  .room-301 .grid-item:nth-child(2) { grid-area: 2 / 1 / 3 / 2; }
  .room-301 .grid-item:nth-child(3) { grid-area: 3 / 1 / 4 / 2; } /* 左3 (ハイライト) */
  .room-301 .grid-item:nth-child(4) { grid-area: 4 / 1 / 5 / 2; }
  .room-301 .grid-item:nth-child(5) { grid-area: 5 / 1 / 6 / 2; }
  .room-301 .grid-item:nth-child(6) { grid-area: 7 / 1 / 8 / 2; } /* 左下1 */
  .room-301 .grid-item:nth-child(7) { grid-area: 8 / 1 / 9 / 2; } /* 左下2 */

  .room-301 .grid-item:nth-child(8) { grid-area: 2 / 2 / 3 / 3; } /* 中央1 */
  .room-301 .grid-item:nth-child(9) { grid-area: 4 / 2 / 5 / 3; } /* 中央2 */

  .room-301 .grid-item:nth-child(10) { grid-area: 1 / 3 / 2 / 4; }
  .room-301 .grid-item:nth-child(11) { grid-area: 2 / 3 / 3 / 4; }
  .room-301 .grid-item:nth-child(12) { grid-area: 3 / 3 / 4 / 4; }
  .room-301 .grid-item:nth-child(13) { grid-area: 4 / 3 / 5 / 4; } /* 右4 (ハイライト) */
  .room-301 .grid-item:nth-child(14) { grid-area: 5 / 3 / 6 / 4; } /* 右5 */
  .room-301 .grid-item:nth-child(15) { grid-area: 7 / 3 / 8 / 4; } /* 右下1 */
  .room-301 .grid-item:nth-child(16) { grid-area: 8 / 3 / 9 / 4; } /* 右下2 */


  /* 2-302 の配置 (5列グリッドベースに修正) */
  /* 左列 (Col 1) */
  .room-302 .grid-item:nth-child(1) { grid-area: 1 / 1 / 2 / 2; }
  .room-302 .grid-item:nth-child(2) { grid-area: 2 / 1 / 3 / 2; }
  .room-302 .grid-item:nth-child(3) { grid-area: 3 / 1 / 4 / 2; }
  .room-302 .grid-item:nth-child(4) { grid-area: 4 / 1 / 5 / 2; } /* 左4 (ハイライト) */
  .room-302 .grid-item:nth-child(5) { grid-area: 5 / 1 / 6 / 2; }

  /* 中央 (Col 3) */
  .room-302 .grid-item:nth-child(6) { grid-area: 3 / 3 / 4 / 4; } /* 中央1 */

  /* 右列 (Col 5) */
  .room-302 .grid-item:nth-child(7) { grid-area: 1 / 5 / 2 / 6; }
  .room-302 .grid-item:nth-child(8) { grid-area: 2 / 5 / 3 / 6; } /* 右2 (ハイライト) */
  .room-302 .grid-item:nth-child(9) { grid-area: 3 / 5 / 4 / 6; }
  .room-302 .grid-item:nth-child(10) { grid-area: 4 / 5 / 5 / 6; }
  .room-302 .grid-item:nth-child(11) { grid-area: 5 / 5 / 6 / 6; }

  /* 下部の5連 (Row 8, Col 1-5) */
  .room-302 .grid-item:nth-child(12) { grid-area: 8 / 1 / 9 / 2; } /* 下1 */
  .room-302 .grid-item:nth-child(13) { grid-area: 8 / 2 / 9 / 3; } /* 下2 */
  .room-302 .grid-item:nth-child(14) { grid-area: 8 / 3 / 9 / 4; } /* 下3 */
  .room-302 .grid-item:nth-child(15) { grid-area: 8 / 4 / 9 / 5; } /* 下4 */
  .room-302 .grid-item:nth-child(16) { grid-area: 8 / 5 / 9 / 6; } /* 下5 */
)rawliteral";

#endif