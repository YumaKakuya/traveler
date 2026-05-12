// PC-6 TypeScript fixture: declaration (interface_declaration, function_declaration),
//                           statement (return_statement),
//                           expression (member_expression, identifier)
interface Point {
    x: number;
}
function dist(p: Point): number {
    return p.x;
}
