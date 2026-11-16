# JUMANJI Enhanced Input assets

Create these assets in Content/Input:

| Asset | Type | Value Type |
|---|---|---|
| IA_Move | Input Action | Axis2D |
| IA_Look | Input Action | Axis2D |
| IA_Jump | Input Action | Boolean |
| IA_Interact | Input Action | Boolean |
| IMC_Player | Input Mapping Context | — |

Map:
- IA_Move: W, A, S, D and Gamepad Left Stick.
- IA_Look: Mouse Delta and Gamepad Right Stick.
- IA_Jump: Space and Gamepad Face Button Bottom.
- IA_Interact: E and Gamepad Face Button Left.

For keyboard movement, use Swizzle Axis to place the W/S one-dimensional values into the Y component of IA_Move. Use Negate for S and A as required by the final axis orientation. Confirm the signs in PIE rather than assuming the default modifier order.

Create BP_JumanjiCharacter from AJumanjiCharacter and assign IMC_Player and all four actions in Class Defaults. Unassigned action properties leave the legacy axis/action bindings active.

The virtual joystick option in DefaultInput.ini is a fallback convenience, not the final multi-touch HUD implementation.
