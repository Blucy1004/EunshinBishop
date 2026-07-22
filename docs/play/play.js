/* Challenge EunshinBishop browser preview.
 * EngineAdapter is intentionally isolated so the heuristic worker can later be
 * replaced by the official EunshinBishop WebAssembly/UCI adapter.
 */
const PIECES={wp:'♟',wn:'♞',wb:'♝',wr:'♜',wq:'♛',wk:'♚',bp:'♟',bn:'♞',bb:'♝',br:'♜',bq:'♛',bk:'♚'};
const VALUES={p:100,n:320,b:330,r:500,q:900,k:0};
const PST={p:[0,0,0,0,0,0,0,0,5,10,10,-20,-20,10,10,5,5,-5,-10,0,0,-10,-5,5,0,0,0,20,20,0,0,0,5,5,10,25,25,10,5,5,10,10,20,30,30,20,10,10,50,50,50,50,50,50,50,50,0,0,0,0,0,0,0,0],n:[-50,-40,-30,-30,-30,-30,-40,-50,-40,-20,0,5,5,0,-20,-40,-30,5,10,15,15,10,5,-30,-30,0,15,20,20,15,0,-30,-30,5,15,20,20,15,5,-30,-30,0,10,15,15,10,0,-30,-40,-20,0,0,0,0,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50]};
const LEVELS={easy:{depth:1,noise:90,delay:260,label:'Easy'},normal:{depth:2,noise:18,delay:420,label:'Normal'},hard:{depth:3,noise:0,delay:650,label:'Hard'}};
const OPENINGS=[
  {eco:'B03',name:'Alekhine Defense: Modern Variation',line:'e4 Nf6 e5 Nd5 d4 d6'},
  {eco:'B03',name:'Alekhine Defense: Four Pawns Attack',line:'e4 Nf6 e5 Nd5 d4 d6 c4 Nb6 f4'},
  {eco:'B02',name:'Alekhine Defense',line:'e4 Nf6'},
  {eco:'B01',name:'Scandinavian Defense',line:'e4 d5'},
  {eco:'B06',name:'Modern Defense',line:'e4 g6'},
  {eco:'B07',name:'Pirc Defense',line:'e4 d6 d4 Nf6 Nc3 g6'},
  {eco:'B10',name:'Caro-Kann Defense',line:'e4 c6'},
  {eco:'B12',name:'Caro-Kann Defense: Advance Variation',line:'e4 c6 d4 d5 e5'},
  {eco:'B20',name:'Sicilian Defense',line:'e4 c5'},
  {eco:'B22',name:'Sicilian Defense: Alapin Variation',line:'e4 c5 c3'},
  {eco:'B23',name:'Sicilian Defense: Closed',line:'e4 c5 Nc3'},
  {eco:'B30',name:'Sicilian Defense: Old Sicilian',line:'e4 c5 Nf3 Nc6'},
  {eco:'B33',name:'Sicilian Defense: Sveshnikov Variation',line:'e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 Nc3 e5'},
  {eco:'B40',name:'Sicilian Defense: French Variation',line:'e4 c5 Nf3 e6'},
  {eco:'B50',name:'Sicilian Defense: Modern Variations',line:'e4 c5 Nf3 d6'},
  {eco:'B90',name:'Sicilian Defense: Najdorf Variation',line:'e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6'},
  {eco:'C00',name:'French Defense',line:'e4 e6'},
  {eco:'C02',name:'French Defense: Advance Variation',line:'e4 e6 d4 d5 e5'},
  {eco:'C10',name:'French Defense: Rubinstein Variation',line:'e4 e6 d4 d5 Nc3 dxe4'},
  {eco:'C20',name:"King's Pawn Game",line:'e4 e5'},
  {eco:'C21',name:'Danish Gambit',line:'e4 e5 d4 exd4 c3'},
  {eco:'C23',name:"Bishop's Opening",line:'e4 e5 Bc4'},
  {eco:'C30',name:"King's Gambit",line:'e4 e5 f4'},
  {eco:'C40',name:"King's Knight Opening",line:'e4 e5 Nf3'},
  {eco:'C42',name:'Petrov Defense',line:'e4 e5 Nf3 Nf6'},
  {eco:'C44',name:'Scotch Game',line:'e4 e5 Nf3 Nc6 d4'},
  {eco:'C45',name:'Scotch Game',line:'e4 e5 Nf3 Nc6 d4 exd4 Nxd4'},
  {eco:'C46',name:'Three Knights Opening',line:'e4 e5 Nf3 Nc6 Nc3'},
  {eco:'C47',name:'Four Knights Game',line:'e4 e5 Nf3 Nc6 Nc3 Nf6'},
  {eco:'C50',name:'Italian Game',line:'e4 e5 Nf3 Nc6 Bc4'},
  {eco:'C54',name:'Italian Game: Giuoco Piano',line:'e4 e5 Nf3 Nc6 Bc4 Bc5'},
  {eco:'C55',name:'Italian Game: Two Knights Defense',line:'e4 e5 Nf3 Nc6 Bc4 Nf6'},
  {eco:'C60',name:'Ruy Lopez',line:'e4 e5 Nf3 Nc6 Bb5'},
  {eco:'C65',name:'Ruy Lopez: Berlin Defense',line:'e4 e5 Nf3 Nc6 Bb5 Nf6'},
  {eco:'A00',name:'Nimzo-Larsen Attack',line:'b3'},
  {eco:'A01',name:"Nimzo-Larsen Attack: Classical Variation",line:'b3 d5 Bb2 Nf6'},
  {eco:'A04',name:'Reti Opening',line:'Nf3'},
  {eco:'A06',name:'Reti Opening',line:'Nf3 d5'},
  {eco:'A10',name:'English Opening',line:'c4'},
  {eco:'A20',name:'English Opening: Kingside Fianchetto',line:'c4 e5 g3'},
  {eco:'A40',name:"Queen's Pawn Game",line:'d4'},
  {eco:'A45',name:'Indian Game',line:'d4 Nf6'},
  {eco:'A57',name:'Benko Gambit',line:'d4 Nf6 c4 c5 d5 b5'},
  {eco:'A60',name:'Benoni Defense',line:'d4 Nf6 c4 c5 d5 e6'},
  {eco:'D00',name:"Queen's Pawn Game",line:'d4 d5'},
  {eco:'D06',name:"Queen's Gambit",line:'d4 d5 c4'},
  {eco:'D10',name:'Slav Defense',line:'d4 d5 c4 c6'},
  {eco:'D20',name:"Queen's Gambit Accepted",line:'d4 d5 c4 dxc4'},
  {eco:'D30',name:"Queen's Gambit Declined",line:'d4 d5 c4 e6'},
  {eco:'D35',name:"Queen's Gambit Declined: Exchange Variation",line:'d4 d5 c4 e6 Nc3 Nf6 cxd5'},
  {eco:'D37',name:"Queen's Gambit Declined",line:'d4 d5 c4 e6 Nc3 Nf6 Nf3'},
  {eco:'E00',name:'Catalan Opening',line:'d4 Nf6 c4 e6 g3'},
  {eco:'E20',name:'Nimzo-Indian Defense',line:'d4 Nf6 c4 e6 Nc3 Bb4'},
  {eco:'E60',name:"King's Indian Defense",line:'d4 Nf6 c4 g6'},
  {eco:'E90',name:"King's Indian Defense: Classical Variation",line:'d4 Nf6 c4 g6 Nc3 Bg7 e4 d6 Nf3 O-O'}
].map(o=>({...o,moves:o.line.split(' ')})).sort((a,b)=>b.moves.length-a.moves.length);
const game=new Chess();
const tr=(key,vars={})=>window.EunshinI18n?.t(key,vars)||key;
const GAME_STATE=Object.freeze({PRE_GAME:'pre-game',STARTING:'starting',PLAYER_TURN:'player-turn',ENGINE_THINKING:'engine-thinking',GAME_OVER:'game-over',REVIEWING:'reviewing'});
let appState=GAME_STATE.PRE_GAME;
let orientation='w',playerColor='w',level='normal',selected=null,legalTargets=[],lastMove=null,hintMove=null,busy=false,pendingAnimationMove=null;
const squareElements=new Map();
let dragState=null,suppressClickUntil=0;
let reviews=[],positionSnapshots=[game.fen()];
const $=s=>document.querySelector(s);

const engine=new EunshinWasmAdapter();

function boardSquares(){const files=orientation==='w'?['a','b','c','d','e','f','g','h']:['h','g','f','e','d','c','b','a'];const ranks=orientation==='w'?[8,7,6,5,4,3,2,1]:[1,2,3,4,5,6,7,8];return ranks.flatMap(rank=>files.map(file=>file+rank));}
function squareDelta(from,to){
  const files=orientation==='w'?['a','b','c','d','e','f','g','h']:['h','g','f','e','d','c','b','a'];
  const ranks=orientation==='w'?[8,7,6,5,4,3,2,1]:[1,2,3,4,5,6,7,8];
  return {dx:files.indexOf(from[0])-files.indexOf(to[0]),dy:ranks.indexOf(Number(from[1]))-ranks.indexOf(Number(to[1]))};
}
function initializeBoard(){
  const board=$('#board');
  if(squareElements.size===64)return;
  board.innerHTML='';
  for(const sq of boardSquares()){
    const file=sq.charCodeAt(0)-97,rank=Number(sq[1]);
    const el=document.createElement('button');
    el.type='button';
    el.className=`square ${((file+rank)%2)?'light':'dark'}`;
    el.dataset.square=sq;
    el.setAttribute('role','gridcell');
    el.setAttribute('aria-label',sq);
    el.addEventListener('click',()=>{if(performance.now()<suppressClickUntil)return;onSquare(el.dataset.square);});
    board.append(el);
    squareElements.set(sq,el);
  }
}
function rebuildBoardOrder(){
  initializeBoard();
  const board=$('#board');
  for(const sq of boardSquares())board.append(squareElements.get(sq));
}
function syncPiece(el,sq,piece,animation){
  let span=el.querySelector('.piece');
  if(!piece){span?.remove();return;}
  if(!span){
    span=document.createElement('span');
    span.className='piece';
    span.setAttribute('draggable','false');
    span.addEventListener('pointerdown',event=>beginPieceDrag(event,span.dataset.from,span));
    el.append(span);
  }
  span.className=`piece ${piece.color==='w'?'white':'black'}`;
  span.textContent=PIECES[piece.color+piece.type];
  span.dataset.from=sq;
  if(animation&&animation.to===sq){
    const {dx,dy}=squareDelta(animation.from,animation.to);
    span.classList.add('piece-slide');
    span.style.setProperty('--move-x',`${dx*100}%`);
    span.style.setProperty('--move-y',`${dy*100}%`);
  }
}
function renderBoard(){
  initializeBoard();
  const animation=pendingAnimationMove;
  pendingAnimationMove=null;
  for(const sq of boardSquares()){
    const el=squareElements.get(sq);
    const file=sq.charCodeAt(0)-97,rank=Number(sq[1]);
    el.className=`square ${((file+rank)%2)?'light':'dark'}`;
    el.dataset.square=sq;
    delete el.dataset.rank;delete el.dataset.file;
    syncPiece(el,sq,game.get(sq),animation);
    if(selected===sq)el.classList.add('selected');
    const legal=legalTargets.find(m=>m.to===sq);
    if(legal)el.classList.add(legal.captured?'capture':'legal');
    if(lastMove&&(lastMove.from===sq||lastMove.to===sq))el.classList.add('last');
    if(hintMove&&(hintMove.from===sq||hintMove.to===sq))el.classList.add('hint');
    if(sq[0]===(orientation==='w'?'a':'h'))el.dataset.rank=sq[1];
    if(sq[1]===(orientation==='w'?'1':'8'))el.dataset.file=sq[0];
  }
  updateEval();
}
function renderAll(){
  renderBoard();
  renderMoves();
  updateOpening();
  $('#engine-label').textContent=`${LEVELS[level].label} · browser engine`;
  const side=game.turn()==='w'?tr('whiteMove'):tr('blackMove');
  $('#turn-label').textContent=game.game_over()?tr('complete'):side;
}
function onSquare(sq){if(!canUseBoard())return;hintMove=null;const piece=game.get(sq);if(selected){const target=legalTargets.find(m=>m.to===sq);if(target){playHuman(target);return;}if(piece&&piece.color===playerColor){selectSquare(sq);return;}selected=null;legalTargets=[];renderBoard();return;}if(piece&&piece.color===playerColor)selectSquare(sq);}
function selectSquare(sq){selected=sq;legalTargets=game.moves({square:sq,verbose:true});renderBoard();}
function beginPieceDrag(event,sq,pieceEl){
  if(event.button!==undefined&&event.button!==0)return;
  if(!canUseBoard())return;
  const piece=game.get(sq);
  if(!piece||piece.color!==playerColor)return;
  event.preventDefault();
  selectSquare(sq);
  const rect=pieceEl.closest('.square').getBoundingClientRect();
  const ghost=document.createElement('span');
  ghost.className=`drag-ghost ${piece.color==='w'?'white':'black'}`;
  ghost.textContent=PIECES[piece.color+piece.type];
  const ghostSize=Math.max(28,Math.round(Math.min(rect.width,rect.height)*0.72));
  ghost.style.width=`${ghostSize}px`;
  ghost.style.height=`${ghostSize}px`;
  ghost.style.fontSize=`${Math.round(ghostSize*0.92)}px`;
  document.body.append(ghost);
  pieceEl.classList.add('drag-source');
  dragState={pointerId:event.pointerId,from:sq,startX:event.clientX,startY:event.clientY,moved:false,ghost,source:pieceEl};
  positionDragGhost(event.clientX,event.clientY);
  pieceEl.setPointerCapture?.(event.pointerId);
  pieceEl.addEventListener('pointermove',movePieceDrag);
  pieceEl.addEventListener('pointerup',endPieceDrag,{once:true});
  pieceEl.addEventListener('pointercancel',cancelPieceDrag,{once:true});
}
function positionDragGhost(x,y){if(!dragState)return;dragState.ghost.style.left=`${x}px`;dragState.ghost.style.top=`${y}px`;}
function movePieceDrag(event){
  if(!dragState||event.pointerId!==dragState.pointerId)return;
  const distance=Math.hypot(event.clientX-dragState.startX,event.clientY-dragState.startY);
  if(distance>4)dragState.moved=true;
  if(dragState.moved)positionDragGhost(event.clientX,event.clientY);
}
function finishDragCleanup(){
  if(!dragState)return;
  dragState.source?.classList.remove('drag-source');
  dragState.ghost?.remove();
  dragState=null;
}
function endPieceDrag(event){
  if(!dragState||event.pointerId!==dragState.pointerId)return;
  const state=dragState;
  const targetEl=document.elementFromPoint(event.clientX,event.clientY)?.closest?.('.square');
  const targetSq=targetEl?.dataset.square;
  finishDragCleanup();
  if(!state.moved)return;
  suppressClickUntil=performance.now()+350;
  const candidate=legalTargets.find(move=>move.to===targetSq);
  if(candidate)playHuman(candidate);
  else{selected=null;legalTargets=[];renderBoard();}
}
function cancelPieceDrag(){finishDragCleanup();}

function nextPaint(){return new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));}
function setAppState(next,options={}){
  appState=next;
  document.body.dataset.gameState=next;
  const overlay=$('#board-overlay'),card=$('#overlay-card');
  const showCard=[GAME_STATE.PRE_GAME,GAME_STATE.GAME_OVER].includes(next);
  overlay.classList.toggle('active',showCard);
  card.hidden=!showCard;
  if(showCard){
    $('#overlay-kicker').textContent=next===GAME_STATE.GAME_OVER?tr('gameComplete'):tr('ready');
    $('#overlay-title').textContent=options.title||(next===GAME_STATE.GAME_OVER?tr('gameOver'):tr('challenge'));
    $('#overlay-text').textContent=options.text||(next===GAME_STATE.GAME_OVER?tr('choose'):tr('choose'));
    $('#overlay-action').textContent=next===GAME_STATE.GAME_OVER?tr('playAgain'):tr('startGame');
  }
  busy=[GAME_STATE.STARTING,GAME_STATE.ENGINE_THINKING].includes(next);
  const board=$('#board');
  const boardEnabled=next===GAME_STATE.PLAYER_TURN;
  board?.classList.toggle('board-disabled',!boardEnabled);
  board?.setAttribute('aria-disabled',String(!boardEnabled));
  if(board)board.inert=!boardEnabled;
  if(!boardEnabled){
    selected=null;
    legalTargets=[];
    cancelPieceDrag();
  }
  $('#new-game').textContent=next===GAME_STATE.PRE_GAME?tr('startGame'):tr('newGame');
}
function canUseBoard(){return appState===GAME_STATE.PLAYER_TURN&&!busy&&!game.game_over()&&game.turn()===playerColor;}
function positionEvalForPlayer(chess){const e=engine.evaluate(chess);return playerColor==='w'?e:-e;}
async function playHuman(candidate){
  if(!canUseBoard())return;
  const before=new Chess(game.fen());
  const evalBefore=positionEvalForPlayer(before);
  const move=game.move({from:candidate.from,to:candidate.to,promotion:'q'});
  if(!move)return;
  selected=null;legalTargets=[];lastMove=move;hintMove=null;
  pendingAnimationMove={from:move.from,to:move.to};
  positionSnapshots.push(game.fen());
  const reviewIndex=reviews.length;
  reviews.push({engine:false,move,classification:'engine',pending:true,pv:[],evalBefore,evalAfter:positionEvalForPlayer(game),loss:0});
  renderAll();
  if(finishIfNeeded())return;
  setAppState(GAME_STATE.REVIEWING);
  setStatus(tr('reviewingMove'),tr('reviewingMove'));
  await nextPaint();
  setTimeout(async()=>{
    const analysis=await engine.scorePlayedMove(before,move,{level:'normal',depth:7});
    const after=new Chess(game.fen());
    const evalAfter=positionEvalForPlayer(after);
    reviews[reviewIndex]=makeReview(move,before,after,evalBefore,evalAfter,analysis);
    renderMoves();
    setTimeout(playEngine,Math.max(80,LEVELS[level].delay-180));
  },0);
}
async function playEngine(){
  if(game.game_over())return;
  setAppState(GAME_STATE.ENGINE_THINKING,{title:tr('thinking'),text:tr('searchingMoves')});
  $('#engine-thinking').classList.add('active');
  setStatus(tr('thinking'),tr('searchingMoves'));
  await nextPaint();
  setTimeout(async()=>{
    const result=await engine.search(game,{level});
    const move=result.move?game.move(result.move):null;
    if(move){lastMove=move;pendingAnimationMove={from:move.from,to:move.to};positionSnapshots.push(game.fen());reviews.push({engine:true,move,classification:'engine',pv:result.pv,evalAfter:positionEvalForPlayer(game)});}
    $('#engine-thinking').classList.remove('active');
    renderAll();
    if(!finishIfNeeded()){
      setAppState(GAME_STATE.PLAYER_TURN);
      setStatus(tr('yourMove'),tr('choosePiece'));
    }
  },40);
}
function makeReview(move,before,after,evalBefore,evalAfter,analysis){
  const loss=Math.max(0,analysis.loss||0);
  const best=analysis.best;
  const reply=analysis.reply;
  const replyMove=reply?.move||null;

  let classification='best';
  if(loss>250)classification='blunder';
  else if(loss>120)classification='mistake';
  else if(loss>60)classification='inaccuracy';
  else if(loss>25)classification='good';
  else if(loss>10)classification='excellent';

  const bestSan=best?.move?.san||null;
  const replySan=replyMove?.san||null;
  const pv=[move.san,...(reply?.pv||[])];
  const explanation=buildMoveExplanation({
    move,before,after,classification,loss,evalBefore,evalAfter,bestSan,replySan,replyMove
  });

  return{
    engine:false,
    move,
    classification,
    loss,
    evalBefore,
    evalAfter,
    pv,
    bestMove:bestSan,
    reply:replySan,
    explanation,
    verified:true
  };
}
function buildMoveExplanation(ctx){
  const {move,classification,loss,evalBefore,evalAfter,bestSan,replySan,replyMove}=ctx;
  const parts=[];
  const isCapture=Boolean(move.captured);
  const isCastle=move.flags.includes('k')||move.flags.includes('q');
  const givesMate=move.san.includes('#');
  const givesCheck=!givesMate&&move.san.includes('+');
  const develops=['n','b'].includes(move.piece)&&['1','8'].includes(move.from[1]);
  const centerPawn=move.piece==='p'&&['d4','e4','d5','e5'].includes(move.to);
  const replyCapturesMovedPiece=Boolean(replyMove&&replyMove.to===move.to&&replyMove.captured===move.piece);
  const evalDelta=evalAfter-evalBefore;
  const piece=pieceName(move.captured||move.piece);

  if(givesMate)parts.push(tr('reviewMate',{move:move.san}));
  else if(isCastle)parts.push(tr('reviewCastle',{move:move.san}));
  else if(givesCheck)parts.push(tr('reviewCheck',{move:move.san}));
  else if(isCapture)parts.push(tr('reviewCapture',{move:move.san,piece}));
  else if(develops)parts.push(tr('reviewDevelop',{move:move.san,piece}));
  else if(centerPawn)parts.push(tr('reviewCenter',{move:move.san}));
  else parts.push(tr('reviewQuiet',{move:move.san}));

  if(replyCapturesMovedPiece){
    const movedPiece=pieceName(move.piece);
    if(loss<=25)parts.push(tr('reviewSacrificeGood',{reply:replySan,piece:movedPiece}));
    else if(loss<=120)parts.push(tr('reviewSacrificePlayable',{reply:replySan,piece:movedPiece}));
    else parts.push(tr('reviewSacrificeBad',{reply:replySan,piece:movedPiece}));
  }else if(replySan){
    parts.push(tr('reviewReply',{reply:replySan}));
  }

  if(classification==='best')parts.push(tr('reviewBest'));
  else if(classification==='excellent')parts.push(tr('reviewExcellent'));
  else if(classification==='good')parts.push(tr('reviewGood',{best:bestSan||'—'}));
  else if(classification==='inaccuracy')parts.push(tr('reviewInaccuracy',{best:bestSan||'—'}));
  else if(classification==='mistake')parts.push(tr('reviewMistake',{best:bestSan||'—'}));
  else parts.push(tr('reviewBlunder',{best:bestSan||'—'}));

  if(Math.abs(evalDelta)>=80){
    parts.push(tr('reviewEval',{
      direction:tr(evalDelta>0?'improves':'worsens'),
      pawns:(Math.abs(evalDelta)/100).toFixed(1)
    }));
  }
  return parts.join(' ');
}
function pieceName(p){return tr(({p:'pawn',n:'knight',b:'bishop',r:'rook',q:'queen',k:'king'})[p]||'piece');}
function normalizedOpeningMoves(){
  return game.history().map(san=>san.replace(/[+#?!]+$/g,'').replace(/^0-0-0$/,'O-O-O').replace(/^0-0$/,'O-O'));
}
function classifyOpening(){
  const moves=normalizedOpeningMoves();
  if(!moves.length)return{eco:'START',name:tr('startingPosition'),variation:tr('openingPending')};
  for(const opening of OPENINGS){
    if(opening.moves.every((move,index)=>moves[index]===move)){
      const variation=moves.length>opening.moves.length?tr('openingKnown'):tr('openingMatched',{count:opening.moves.length});
      return{eco:opening.eco,name:opening.name,variation};
    }
  }
  const first=moves[0];
  const fallback=first==='e4'?['B00',"King's Pawn Opening"]:first==='d4'?['A40',"Queen's Pawn Opening"]:first==='c4'?['A10','English Opening']:first==='Nf3'?['A04','Reti Opening']:first==='b3'?['A00','Nimzo-Larsen Attack']:['A00','Uncommon Opening'];
  return{eco:fallback[0],name:fallback[1],variation:tr('openingUnknown')};
}
function updateOpening(){
  const opening=classifyOpening();
  $('#opening-eco').textContent=opening.eco;
  $('#opening-name').textContent=opening.name;
  $('#opening-variation').textContent=opening.variation;
  $('#opening-card').title=`${opening.eco} · ${opening.name}`;
}
function updateEval(){const e=Math.max(-1200,Math.min(1200,engine.evaluate(game)));const whitePct=50+(e/1200)*45;$('#eval-fill').style.height=`${100-whitePct}%`;$('#eval-label').textContent=Math.abs(e)>=29000?(e>0?'M':'-M'):(e/100).toFixed(1);}
function renderMoves(){const list=$('#move-list');const hist=game.history({verbose:true});if(!hist.length){list.innerHTML=`<p class="empty">${tr('noMoves')}</p>`;return;}list.innerHTML='';for(let i=0;i<hist.length;i+=2){const row=document.createElement('div');row.className='move-row';row.innerHTML=`<span>${i/2+1}.</span>${moveCell(hist[i],i)}${hist[i+1]?moveCell(hist[i+1],i+1):'<span></span>'}`;list.append(row);}list.scrollTop=list.scrollHeight;list.querySelectorAll('.move-cell').forEach(el=>el.onclick=()=>showReview(Number(el.dataset.index)));}
function moveCell(m,i){const r=reviews[i],cls=r?.pending?'engine':(r&&!r.engine?r.classification:'engine');return `<span class="move-cell" data-index="${i}"><i class="dot ${cls}" style="background:${badgeColor(cls)}"></i>${m.san}</span>`;}
function badgeColor(c){return({best:'#6fdcb1',excellent:'#86d6db',good:'#a8cf70',inaccuracy:'#f0cd70',mistake:'#efa067',blunder:'#f2788d',engine:'#a67cff'})[c]||'#777';}
function showReview(i){
  const r=reviews[i];
  document.querySelector('[data-tab="review"]').click();
  if(!r)return;
  if(r.engine){
    $('#review-badge').className='move-badge neutral';
    $('#review-badge').textContent='Engine';
    $('#review-title').textContent=r.move.san;
    $('#review-eval').textContent=tr('engineMove');
    $('#review-explanation').textContent='This move was selected by the current browser engine at the chosen difficulty.';
    $('#review-pv').textContent=(r.pv||[]).join(' ')||'—';
    return;
  }
  $('#review-badge').className=`move-badge ${r.classification}`;
  $('#review-badge').textContent=r.classification;
  $('#review-title').textContent=r.move.san;
  $('#review-eval').textContent=`Evaluation: ${(r.evalBefore/100).toFixed(1)} → ${(r.evalAfter/100).toFixed(1)} · loss ${Math.round(r.loss)} cp`;
  $('#review-explanation').textContent=r.explanation||tr('noExplanation');
  $('#review-pv').textContent=r.pv.join(' ')||'—';
}
function finishIfNeeded(){if(!game.game_over())return false;let title='Game over',text='The game has ended.';if(game.in_checkmate()){title=game.turn()===playerColor?'Checkmate — EunshinBishop wins':'Checkmate — You win';text='The king has no legal escape.';}else if(game.in_stalemate()){title='Draw by stalemate';text='The side to move has no legal move.';}else if(game.in_threefold_repetition()){title='Draw by repetition';text='The same position occurred three times.';}else if(game.insufficient_material()){title='Draw by insufficient material';text='There is not enough material to checkmate.';}else if(game.in_draw()){title='Draw';text='The position is drawn.';}setStatus(title,text);setAppState(GAME_STATE.GAME_OVER,{title,text});return true;}
function setStatus(title,text){$('#status-title').textContent=title;$('#status-text').textContent=text;}
async function newGame(){
  setAppState(GAME_STATE.STARTING,{title:'Preparing the board…',text:'Resetting the position and player settings'});
  setStatus('Preparing game','Setting up a fresh position…');
  await nextPaint();
  setTimeout(()=>{
    game.reset();reviews=[];positionSnapshots=[game.fen()];selected=null;legalTargets=[];lastMove=null;hintMove=null;pendingAnimationMove=null;
    rebuildBoardOrder();renderAll();
    if(playerColor==='b'){
      setStatus('EunshinBishop moves first','The engine is preparing its opening move.');
      setTimeout(playEngine,120);
    }else{
      setAppState(GAME_STATE.PLAYER_TURN);
      setStatus(tr('yourMove'),tr('choosePiece'));
    }
  },90);
}
function undoTurn(){if(![GAME_STATE.PLAYER_TURN,GAME_STATE.GAME_OVER].includes(appState))return;let undone=0;while(game.history().length&&undone<2){game.undo();reviews.pop();positionSnapshots.pop();undone++;}selected=null;legalTargets=[];lastMove=null;hintMove=null;pendingAnimationMove=null;renderAll();setAppState(GAME_STATE.PLAYER_TURN);setStatus('Turn undone','The previous player and engine moves were restored.');}
async function requestHint(){
  if(!canUseBoard())return;
  setAppState(GAME_STATE.REVIEWING);
  setStatus(tr('hint'),tr('searchingMoves'));
  await nextPaint();
  try{
    const result=await engine.search(game,{level:'normal'});
    if(!result.move)return;
    hintMove=result.move;
    $('#hint-box').hidden=false;
    $('#hint-move').textContent=result.move.san;
    const reply=result.pv?.[1]||'—';
    $('#hint-text').textContent=tr('expectedReply',{reply});
    renderBoard();
  }catch(error){
    setStatus('Engine error',String(error.message||error));
  }finally{
    setAppState(GAME_STATE.PLAYER_TURN);
  }
}
function resetPreGamePreview(){
  game.reset();reviews=[];positionSnapshots=[game.fen()];selected=null;legalTargets=[];lastMove=null;hintMove=null;pendingAnimationMove=null;
  rebuildBoardOrder();renderAll();
  setAppState(GAME_STATE.PRE_GAME);
  setStatus(tr('ready'),tr('choose'));
}
function bind(){document.querySelectorAll('.tab').forEach(btn=>btn.onclick=()=>{document.querySelectorAll('.tab,.tab-page').forEach(x=>x.classList.remove('active'));btn.classList.add('active');$(`#tab-${btn.dataset.tab}`).classList.add('active');});$('#difficulty').onclick=e=>{const b=e.target.closest('button');if(!b)return;level=b.dataset.level;document.querySelectorAll('#difficulty button').forEach(x=>x.classList.toggle('active',x===b));renderAll();};$('#color-choice').onclick=e=>{const b=e.target.closest('button');if(!b)return;playerColor=b.dataset.color;orientation=playerColor;document.querySelectorAll('#color-choice button').forEach(x=>x.classList.toggle('active',x===b));rebuildBoardOrder();renderBoard();if(appState!==GAME_STATE.PRE_GAME)setAppState(GAME_STATE.PRE_GAME);};$('#new-game').onclick=()=>{if(appState===GAME_STATE.PRE_GAME)newGame();else resetPreGamePreview();};$('#overlay-action').onclick=newGame;$('#undo').onclick=undoTurn;$('#hint').onclick=requestHint;$('#flip').onclick=()=>{orientation=orientation==='w'?'b':'w';rebuildBoardOrder();renderBoard();};$('#copy-pgn').onclick=async()=>{try{await navigator.clipboard.writeText(game.pgn());setStatus(tr('pgnCopied'),tr('pgnCopied'));}catch{setStatus(tr('copyUnavailable'),tr('copyUnavailable'));}};}
async function loadLiveStatus(){try{const [repoRes,relRes]=await Promise.all([fetch('https://api.github.com/repos/Blucy1004/EunshinBishop'),fetch('https://api.github.com/repos/Blucy1004/EunshinBishop/releases/latest')]);if(repoRes.ok){const repo=await repoRes.json();$('#repo-status').textContent=`★ ${repo.stargazers_count} · ${repo.open_issues_count} issues`;}else throw new Error();if(relRes.ok){const rel=await relRes.json();$('#release-name').textContent=rel.name||rel.tag_name;$('#release-date').textContent=new Date(rel.published_at).toLocaleDateString();$('#release-link').href=rel.html_url;}else{$('#release-name').textContent='Development build';$('#release-date').textContent='No tagged release';}}catch{$('#repo-status').textContent='Offline';$('#release-name').textContent='EunshinBishop Q';$('#release-date').textContent='Status unavailable';}}
if(typeof Chess==='undefined'){document.body.innerHTML='<main style="padding:40px;color:white;font-family:system-ui"><h1>Chess library failed to load.</h1><p>Serve this page with an internet connection or vendor chess.js locally.</p></main>';}else{bind();initializeBoard();rebuildBoardOrder();renderAll();setAppState(GAME_STATE.PRE_GAME);setStatus(tr('ready'),tr('choose'));loadLiveStatus();}

document.addEventListener('eunshin:languagechange',()=>{
  reviews.forEach(r=>{
    if(r && !r.engine && r.move){
      r.explanation=buildMoveExplanation({
        move:r.move,
        classification:r.classification,
        loss:r.loss||0,
        evalBefore:r.evalBefore||0,
        evalAfter:r.evalAfter||0,
        bestSan:r.bestMove||null,
        replySan:r.reply||null,
        replyMove:null
      });
    }
  });
  renderAll();
  setAppState(appState);
  if(appState===GAME_STATE.PRE_GAME) setStatus(tr('ready'),tr('choose'));
});
